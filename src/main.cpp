#include "constants.h"
#include "type.h"

#include <lib/misc/button.h>
#include <lib/misc/led.h>
#include <lib/misc/vector.h>
#include <lib/network/now_io.h>

enum class ApplicationState: uint8_t {
    BUTTON_WAIT,

    NETWORK_INITIALIZATION,
    DISCOVERY,

    DATA_SENDING,

    FINISHED,
    RESULT_INDICATION,

    TURNING_OFF,
    END
};

enum class CommandState: uint8_t {
    UNKNOWN,

    SUCCESS,
    NOTHING_TO_SEND,

    HUB_MISSING,
    SEND_TIMEOUT,
    SEND_ERROR
};

ApplicationState state = ApplicationState::BUTTON_WAIT;
CommandState command_state = CommandState::UNKNOWN;
unsigned long initialized_time = 0;

Led led(PIN_LED);
Vector<Button> buttons;

auto &now = NowIo::instance();

RTC_DATA_ATTR bool hub_addr_present = false;
RTC_DATA_ATTR uint8_t hub_addr[6] {};
RTC_DATA_ATTR static uint8_t wifi_channel = 0;
RTC_DATA_ATTR uint8_t error_count = 0;

void initialize_debugger();
void state_machine();

void setup() {
    VERBOSE(initialize_debugger());

    buttons.reserve(BUTTON_COUNT);
    for (int i = 0; i < BUTTON_COUNT; ++i) {
        auto &button = buttons.emplace(BUTTON_PINS[i], BTN_HIGH_STATE, true);
        button.begin(BTN_MODE);
    }

    led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
    led.begin();

    led.set_brightness(0xA0);
    led.flash();

    if (error_count >= SEND_ERROR_BEFORE_RESET) {
        D_PRINT("Too many sending errors. Reseting saved HUB...");

        hub_addr_present = false;
        memset(hub_addr, 0, sizeof(hub_addr));
        wifi_channel = 0;
        error_count = 0;
    }

    initialized_time = millis();
}

void loop() {
    ApplicationState prev_state;

    do {
        prev_state = state;
        state_machine();
    } while (prev_state != state);

    for (auto &button: buttons) button.handle();
    led.tick();

    delay(DELAY_AMOUNT);
}

void state_machine() {
    switch (state) {
        case ApplicationState::BUTTON_WAIT: {
            bool all_idle = std::all_of(buttons.begin(), buttons.end(), [](auto &b) { return b.idle(); });
            if (all_idle && millis() - initialized_time > BUTTON_WAIT_TIMEOUT) {
                state = ApplicationState::NETWORK_INITIALIZATION;
            }
            break;
        }

        case ApplicationState::NETWORK_INITIALIZATION:
#ifdef DEBUG
            initialize_debugger();
#endif

            now.begin();
            AsyncEspNowInteraction::print_mac();

            if (!hub_addr_present) {
                state = ApplicationState::DISCOVERY;
                break;
            }

            AsyncEspNow::instance().change_channel(wifi_channel);
            state = ApplicationState::DATA_SENDING;
            break;

        case ApplicationState::DISCOVERY: {
            if (!hub_addr_present) {
                auto discovery_future = now.discover_hub(hub_addr);
                if (!discovery_future.wait(DISCOVERY_TIMEOUT) || !discovery_future.success()) {
                    D_PRINT("*** Unable to find HUB. Exit...");
                    command_state = CommandState::HUB_MISSING;
                    state = ApplicationState::FINISHED;
                    break;
                }

                hub_addr_present = true;
                wifi_channel = discovery_future.result();

                state = ApplicationState::DATA_SENDING;
            }

            break;
        }

        case ApplicationState::DATA_SENDING: {
            bool nothing_to_send = std::all_of(buttons.begin(), buttons.end(),
                [](auto &b) { return b.last_state().click_count == 0; });

            if (nothing_to_send) {
                D_PRINT("Nothing to send");

                state = ApplicationState::FINISHED;
                command_state = CommandState::NOTHING_TO_SEND;
                break;
            }

            std::vector<ButtonEvent> button_events(BUTTON_COUNT);
            for (int i = 0; i < BUTTON_COUNT; ++i) {
                auto &btn_state = buttons[i].last_state();
                button_events[i] = {
                    .event_type = btn_state.hold ? ButtonEventType::HOLD : ButtonEventType::CLICKED,
                    .click_count = btn_state.click_count
                };

                D_PRINTF("Button #%i: Type: %s, Count %i\r\n", i, btn_state.hold ? "Hold" : "Click", btn_state.click_count);
            }

            auto send_future = now.send(hub_addr, (uint8_t) PacketType::BUTTON, button_events);
            if (!send_future.wait(SEND_TIMEOUT) || !send_future.success()) {
                D_PRINTF("Failed to send message: %s\r\n", send_future.finished() ? "error" : "timeout");
                command_state = send_future.finished() ? CommandState::SEND_ERROR : CommandState::SEND_TIMEOUT;
                ++error_count;
                state = ApplicationState::FINISHED;
                break;
            }

            command_state = CommandState::SUCCESS;
            error_count = 0;
            state = ApplicationState::FINISHED;
            break;
        }

        case ApplicationState::FINISHED: {
            auto buttons_not_idle = std::any_of(BUTTON_PINS, BUTTON_PINS + BUTTON_COUNT,
                [](auto pin) { return digitalRead(pin) == HIGH; });

            if (buttons_not_idle) break;

            for (auto &button: buttons) button.end();
            state = ApplicationState::RESULT_INDICATION;
            break;
        }

        case ApplicationState::RESULT_INDICATION: {
            led.turn_off();

            bool need_indication = false;
            uint8_t blink_count = 0;
            switch (command_state) {
                case CommandState::HUB_MISSING:
                    blink_count = 5;
                    need_indication = true;
                    break;
                case CommandState::SEND_TIMEOUT:
                    blink_count = 4;
                    need_indication = true;
                    break;
                case CommandState::SEND_ERROR:
                    blink_count = 3;
                    need_indication = true;
                    break;

                default:;
            }

            if (need_indication) {
                delay(led.blink_repeat_interval());
                led.blink(blink_count, false);

                while (led.active()) {
                    led.tick();
                    delay(DELAY_AMOUNT);
                }
            }

            state = ApplicationState::TURNING_OFF;
            break;
        }

        case ApplicationState::TURNING_OFF: {
            led.turn_off();

            D_PRINTF("Finished with result: %u", command_state);
            state = ApplicationState::END;

            uint64_t button_mask = 0;
            for (int i = 0; i < BUTTON_COUNT; ++i) button_mask |= 1 << BUTTON_PINS[i];

            esp_deep_sleep_enable_gpio_wakeup(button_mask, ESP_GPIO_WAKEUP_GPIO_HIGH);
            esp_deep_sleep_start();
            break;
        }

        case ApplicationState::END:
            D_PRINT("You shouldn't be here 0_0");
            break;
    }
}

void initialize_debugger() {
    static bool debugger_initialized = false;
    if (debugger_initialized) return;

    Serial.begin(115200);
    debugger_initialized = true;

    auto start_t = millis();
    while (!Serial && millis() - start_t < 15000ul) delay(100);

    delay(2000);
}
