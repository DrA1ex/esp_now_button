#include "constants.h"

#include "lib/misc/button.h"
#include "lib/misc/led.h"
#include "lib/network/now.h"

enum class ApplicationState {
    INITIALIZATION,

    BUTTON_LISTENING,
    BUTTON_RELEASE_WAITING,

    NETWORK_INITIALIZATION,
    DATA_SENDING,
    DATA_ACK_WAITING,

    FINISHED,
    RESULT_INDICATION,

    TURNING_OFF,
    END
};

ApplicationState state = ApplicationState::INITIALIZATION;
unsigned long initialized_time = 0;
bool command_success = false;

Led led(PIN_LED);
Button button1(PIN_BUTTON1, true, true);
Button button2(PIN_BUTTON2, true, true);

EspNow &esp_now = EspNow::instance();

void initialize_debugger() {
    Serial.begin(115200);

    auto start_t = millis();
    while (!Serial && (millis() - start_t) < 15000ul) delay(100);

    delay(2000);
}

void state_machine() {
    switch (state) {
        case ApplicationState::INITIALIZATION:
            led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
            led.begin();
            led.flash();

            button1.begin(INPUT_PULLDOWN);
            button2.begin(INPUT_PULLDOWN);

            state = ApplicationState::BUTTON_LISTENING;
            break;

        case ApplicationState::BUTTON_LISTENING:
            if (millis() >= BUTTON_WAIT_TIMEOUT) {
                if (button1.idle() && button2.idle()) {
                    state = ApplicationState::NETWORK_INITIALIZATION;
                } else {
                    state = ApplicationState::BUTTON_RELEASE_WAITING;
                }
            }
            break;

        case ApplicationState::BUTTON_RELEASE_WAITING:
            if (button1.idle() && button2.idle()) {
                state = ApplicationState::NETWORK_INITIALIZATION;
            }
            break;

        case ApplicationState::NETWORK_INITIALIZATION:
#ifdef DEBUG
            initialize_debugger();
#endif

            esp_now.begin();
            esp_now.print_mac();

            if (!esp_now.configure_channel(HUB_MAC_ADDR)) {
                D_PRINT("*** Unable to find HUB's channel. Exit...");
                state = ApplicationState::FINISHED;
                break;
            }

            initialized_time = millis();
            state = ApplicationState::DATA_SENDING;
            break;

        case ApplicationState::DATA_SENDING: {
            if (button1.last_state().click_count == 0 && button2.last_state().click_count == 0) {
                state = ApplicationState::FINISHED;
                command_success = true;
                D_PRINT("Nothing to send");
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

            esp_now.set_on_sent([](auto, auto status) {
                D_PRINTF("Packet sent, status: %x\r\n", status);
                command_success = status == ESP_NOW_SEND_SUCCESS;
                state = ApplicationState::FINISHED;
            });

            bool success = esp_now.send_packet(HUB_MAC_ADDR, PacketType::BUTTON, events);
            if (!success) {
                D_PRINT("Failed to send message");
                state = ApplicationState::FINISHED;
                break;
            }

            state = ApplicationState::DATA_ACK_WAITING;
        }
            break;

        case ApplicationState::DATA_ACK_WAITING:
            if (millis() - initialized_time > SEND_TIMEOUT) {
                D_PRINT("Sending timeout");
                state = ApplicationState::FINISHED;
            }
            break;

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

            state = ApplicationState::END;

            esp_deep_sleep_enable_gpio_wakeup((1 << PIN_BUTTON1) | (1 << PIN_BUTTON2), ESP_GPIO_WAKEUP_GPIO_HIGH);
            esp_deep_sleep_start();

        case ApplicationState::END:
            D_PRINT("You shouldn't be here 0_0");
            break;
    }
}

void setup() {}

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
