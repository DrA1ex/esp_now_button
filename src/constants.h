#pragma once

#include <array>
#include <cstdint>

constexpr uint8_t BUTTON_COUNT = 2;
constexpr std::array<uint8_t, BUTTON_COUNT> BUTTON_PINS = {0, 1};

constexpr uint8_t LED_PIN = 3;
constexpr unsigned long LED_HOLDING_BLINK_INTERVAL = 200;

constexpr unsigned long DISCOVERY_TIMEOUT = 5000;

constexpr unsigned long SEND_TIMEOUT = 300;
constexpr uint8_t SEND_ERROR_BEFORE_RESET = 3;
constexpr uint8_t SEND_RETRY_COUNT = 2;
constexpr uint8_t SEND_RETRY_DELAY = 100;

constexpr unsigned long DELAY_AMOUNT = 10;

constexpr unsigned long BUTTON_WAIT_TIMEOUT = 600;
constexpr unsigned long BUTTON_REPEAT_TIMEOUT = 1000;

#define BTN_MODE                                (INPUT_PULLDOWN)
#define BTN_HIGH_STATE                          (true)

#define BTN_SILENCE_INTERVAL                    (5u)
#define BTN_HOLD_INTERVAL                       (600u)
#define BTN_PRESS_WAIT_INTERVAL                 (600u)
#define BTN_RESET_INTERVAL                      (1000u)
