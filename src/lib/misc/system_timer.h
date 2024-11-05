#pragma once

#include <Arduino.h>
#include <functional>
#include <queue>

#include "promise.h"

class SystemTimer {
    struct TimerTaskParams;

    static portMUX_TYPE spinlock;
    static bool task_running;

    typedef std::priority_queue<TimerTaskParams, std::vector<TimerTaskParams>, std::greater<>> PriorityQueue;
    static PriorityQueue timers;

public:
    typedef std::function<void()> CallbackType;

    static Future<void> delay(unsigned long timeout_ms);
    static bool set_timeout(unsigned long timeout_ms, CallbackType callback);

private:
    struct TimerTaskParams {
        uint64_t timeout_at;
        CallbackType callback;

        bool operator>(const TimerTaskParams &other) const { return timeout_at > other.timeout_at; }
    };

    static bool start_task();
    static void timer_task(void *arg);

    // Actually it's ~ 53 bits, but it doesn't really matter...
    static uint64_t millis64() { return esp_timer_get_time() / 1000; };
};
