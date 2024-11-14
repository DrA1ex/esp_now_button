#pragma once

#include <cstdint>

constexpr uint8_t BUTTON_COUNT = 2;
constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {0, 1};

constexpr uint8_t PIN_LED = 3;

constexpr unsigned long DISCOVERY_TIMEOUT = 5000;
constexpr unsigned long SEND_TIMEOUT = 500;
constexpr unsigned long BUTTON_WAIT_TIMEOUT = 600;

constexpr unsigned long DELAY_AMOUNT = 10;

constexpr unsigned long LED_HOLDING_BLINK_INTERVAL = 200;

constexpr uint8_t SEND_ERROR_BEFORE_RESET = 3;

#define BTN_MODE                                (INPUT_PULLDOWN)
#define BTN_HIGH_STATE                          (true)

#define BTN_SILENCE_INTERVAL                    (5u)
#define BTN_HOLD_INTERVAL                       (600u)
#define BTN_PRESS_WAIT_INTERVAL                 (600u)
#define BTN_RESET_INTERVAL                      (1000u)
