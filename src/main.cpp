#include "constants.h"
#include "type.h"

#include <lib/misc/button.h>
#include <lib/misc/led.h>
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
Button button1(PIN_BUTTON1, true, true);
Button button2(PIN_BUTTON2, true, true);

auto &now = NowIo::instance();

RTC_DATA_ATTR bool hub_addr_present = false;
RTC_DATA_ATTR uint8_t hub_addr[6] {};
RTC_DATA_ATTR static uint8_t wifi_channel = 0;
RTC_DATA_ATTR uint8_t error_count = 0;

void initialize_debugger();
void state_machine();

void setup() {
    led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
    led.begin();
    led.set_brightness(0xA0);

    button1.begin(INPUT_PULLDOWN);
    button2.begin(INPUT_PULLDOWN);

    led.flash();

    if (error_count >= SEND_ERROR_BEFORE_RESET) {
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

    button1.handle();
    button2.handle();
    led.tick();

    delay(DELAY_AMOUNT);
}

void state_machine() {
    switch (state) {
        case ApplicationState::BUTTON_WAIT:
            if (button1.idle() && button2.idle() && millis() - initialized_time > BUTTON_WAIT_TIMEOUT) {
                state = ApplicationState::NETWORK_INITIALIZATION;
            }
            break;

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
            if (button1.last_state().click_count == 0 && button2.last_state().click_count == 0) {
                D_PRINT("Nothing to send");

                state = ApplicationState::FINISHED;
                command_state = CommandState::NOTHING_TO_SEND;
                break;
            }

            ButtonEvent events[] = {
                {
                    .event_type = button1.last_state().hold ? ButtonEventType::HOLD : ButtonEventType::CLICKED,
                    .click_count = button1.last_state().click_count
                },
                {
                    .event_type = button2.last_state().hold ? ButtonEventType::HOLD : ButtonEventType::CLICKED,
                    .click_count = button2.last_state().click_count
                }
            };

            auto send_future = now.send(hub_addr, (uint8_t) PacketType::BUTTON, events);
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

        case ApplicationState::FINISHED:
            if (digitalRead(PIN_BUTTON1) || digitalRead(PIN_BUTTON2)) {
                break;
            }

            button1.end();
            button2.end();
            state = ApplicationState::RESULT_INDICATION;
            break;

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

        case ApplicationState::TURNING_OFF:
            led.turn_off();

            D_PRINTF("Finished with result: %u", command_state);
            state = ApplicationState::END;

            esp_deep_sleep_enable_gpio_wakeup((1 << PIN_BUTTON1) | (1 << PIN_BUTTON2), ESP_GPIO_WAKEUP_GPIO_HIGH);
            esp_deep_sleep_start();
            break;

        case ApplicationState::END:
            D_PRINT("You shouldn't be here 0_0");
            break;
    }
}

void initialize_debugger() {
    Serial.begin(115200);

    auto start_t = millis();
    while (!Serial && millis() - start_t < 15000ul) delay(100);

    delay(2000);
}
