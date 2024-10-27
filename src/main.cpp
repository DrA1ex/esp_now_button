#include "constants.h"

#include "lib/misc/button.h"
#include "lib/misc/led.h"
#include "lib/network/now.h"

Led led(PIN_LED);
Button button1(PIN_BUTTON1, true, true);
Button button2(PIN_BUTTON2, true, true);

EspNow &esp_now = EspNow::instance();

bool command_finished = false;
bool command_success = false;
unsigned long initialized_time = 0;

void setup() {
    led.set_blink_repeat_interval(LED_HOLDING_BLINK_INTERVAL);
    led.begin();

    button1.begin(INPUT_PULLDOWN);
    button2.begin(INPUT_PULLDOWN);

    led.flash();

    while (millis() < BUTTON_WAIT_TIMEOUT || !button1.idle() || !button2.idle()) {
        button1.handle();
        button2.handle();
        led.tick();

        delay(DELAY_AMOUNT);

        if (button1.last_state().hold || button2.last_state().hold && led.blink_count() == 0 && millis() < BUTTON_WAIT_TIMEOUT) {
            led.blink(1, true);
        }
    }

    led.flash();

#ifdef DEBUG
    Serial.begin(115200);
    {
        auto start_t = millis();
        while (!Serial && (millis() - start_t) < 15000ul) delay(100);

        delay(2000);
    }
#endif

    auto wakeup_status = esp_sleep_get_gpio_wakeup_status();

    D_WRITE("Wake up pins: ");
    D_PRINT_BIN(wakeup_status);

    D_PRINTF("Button 1: %s %i at %u\n", button1.last_state().hold ? "HOLD" : "CLICK",
             button1.last_state().click_count, button1.last_state().timestamp);

    D_PRINTF("Button 2: %s %i at %u\n", button2.last_state().hold ? "HOLD" : "CLICK",
             button2.last_state().click_count, button2.last_state().timestamp);

    D_PRINT("*** Starting... ***");

    esp_now.begin();
    esp_now.print_mac();

    if (!esp_now.configure_channel(HUB_MAC_ADDR)) {
        D_PRINT("*** Unable to find HUB's channel. Exit...");
        command_finished = true;
        return;
    }

    esp_now.set_on_sent([](auto, auto status) {
        D_PRINTF("Packet sent, status: %x\r\n", status);
        command_success = status == ESP_NOW_SEND_SUCCESS;
        command_finished = true;
    });

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

    bool success = esp_now.send_packet(HUB_MAC_ADDR, PacketType::BUTTON, events);
    if (!success) {
        D_PRINT("Failed to send message");
        command_finished = true;
    }
}

void loop() {
    if (initialized_time == 0) initialized_time = millis();

    led.tick();

    if (!command_finished && millis() - initialized_time > SEND_TIMEOUT) {
        D_PRINT("Sending timeout");
        command_finished = true;
    }

    if (command_finished) {
        if (digitalRead(PIN_BUTTON1) || digitalRead(PIN_BUTTON2)) {
            if (led.blink_count() == 0) {
                led.blink(2, true);
            }

            delay(DELAY_AMOUNT);
            return;
        }

        button1.end();
        button2.end();

        if (!command_success) {
            led.turn_off();
            delay(led.blink_repeat_interval());

            led.blink(3, false);
            while (led.active()) {
                led.tick();
                delay(DELAY_AMOUNT);
            }
        }

        D_PRINT("*** Going to sleep... ***");

        led.turn_off();

        esp_deep_sleep_enable_gpio_wakeup((1 << PIN_BUTTON1) | (1 << PIN_BUTTON2), ESP_GPIO_WAKEUP_GPIO_HIGH);
        esp_deep_sleep_start();
    }

    delay(DELAY_AMOUNT);
}
