#include "constants.h"
#include "type.h"

#include <lib/misc/button.h>
#include <lib/misc/led.h>
#include <lib/network/now_io.h>

enum class ApplicationState {
    BUTTON_WAIT,

    NETWORK_INITIALIZATION,
    DISCOVERY,

    DATA_SENDING,

    FINISHED,
    RESULT_INDICATION,

    TURNING_OFF,
    END
};

ApplicationState state = ApplicationState::BUTTON_WAIT;
unsigned long initialized_time = 0;
bool command_success = false;

Led led(PIN_LED);
Button button1(PIN_BUTTON1, true, true);
Button button2(PIN_BUTTON2, true, true);

auto &now = NowIo::instance();

RTC_DATA_ATTR bool hub_addr_present = false;
RTC_DATA_ATTR uint8_t hub_addr[6] = {0xCC, 0xDB, 0xA7, 0x14, 0x25, 0xF8};
RTC_DATA_ATTR static uint8_t wifi_channel = 0;

void initialize_debugger();
void state_machine();

void setup() {
    // initialize_debugger();
    //
    // now.begin();
    //
    // auto discovery_future = now.discover_hub(hub_addr);
    // if (!discovery_future.wait(5000) || !discovery_future.success()) {
    //     D_PRINT("*** Unable to find HUB. Exit...");
    //     return;
    // }
    //
    // D_PRINTF("*** HUB found: channel %i, addr: ", discovery_future.result() + 1);
    // D_PRINT_HEX(hub_addr, 6);
    // return;

    led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
    led.begin();

    button1.begin(INPUT_PULLDOWN);
    button2.begin(INPUT_PULLDOWN);

    led.flash();

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
                if (!discovery_future.wait(5000) || !discovery_future.success()) {
                    D_PRINT("*** Unable to find HUB. Exit...");
                    command_success = false;
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
                command_success = true;
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

            auto send_future = now.send_packet(hub_addr, (uint8_t) PacketType::BUTTON, events);
            if (!send_future.wait(500) || !send_future.success()) {
                D_PRINT("Failed to send message");
                command_success = false;
                state = ApplicationState::FINISHED;
                break;
            }

            command_success = true;
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

        case ApplicationState::RESULT_INDICATION:
            led.turn_off();

            if (!command_success) {
                delay(led.blink_repeat_interval());
                led.blink(3, false);

                while (led.active()) {
                    led.tick();
                    delay(DELAY_AMOUNT);
                }
            }

            state = ApplicationState::TURNING_OFF;
            break;

        case ApplicationState::TURNING_OFF:
            led.turn_off();

            D_PRINTF("Finished with result: %s", command_success ? "ok" : "error");
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
