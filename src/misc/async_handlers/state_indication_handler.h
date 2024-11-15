#pragma once

#include "base/async_handler.h"

class StateIndicationHandler : public AsyncHandlerBase {
public:
    void blink(Led &led, uint8_t blink_count);
};

inline void StateIndicationHandler::blink(Led &led, uint8_t blink_count) {
    _start([&] {
        led.turn_off();
        led.blink(blink_count, false);

        auto wait_interval = (led.blink_active_duration() + led.blink_wait_duration()) * blink_count;
        return SystemTimer::delay(wait_interval);
    }, 0);
}
