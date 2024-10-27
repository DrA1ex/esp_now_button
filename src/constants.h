#pragma once

#include <cstdint>

constexpr uint8_t PIN_BUTTON1 = 0;
constexpr uint8_t PIN_BUTTON2 = 1;

constexpr uint8_t PIN_LED = 3;

constexpr uint8_t HUB_MAC_ADDR[] = {0xCC, 0xDB, 0xA7, 0x14, 0x25, 0xF8};


constexpr unsigned long SEND_TIMEOUT = 3000;
constexpr unsigned long BUTTON_WAIT_TIMEOUT = 600;

constexpr unsigned long DELAY_AMOUNT = 10;

constexpr unsigned long LED_HOLDING_BLINK_INTERVAL = 200;

#define BTN_SILENCE_INTERVAL                    (5u)
#define BTN_HOLD_INTERVAL                       (600u)
#define BTN_PRESS_WAIT_INTERVAL                 (600u)
#define BTN_RESET_INTERVAL                      (1000u)

