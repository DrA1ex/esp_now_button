#pragma once

#include <Arduino.h>
#include <functional>
#include <queue>

#include "promise.h"

#ifndef SYSTEM_TIMER_STACK_SIZE
#define SYSTEM_TIMER_STACK_SIZE                             (4096u)
#endif

#ifndef SYSTEM_TIMER_TASK_PRIORITY
#define SYSTEM_TIMER_TASK_PRIORITY                          (1u)
#endif

#ifndef SYSTEM_TIMER_DELAY_INTERVAL_MICRO
#define SYSTEM_TIMER_DELAY_INTERVAL_MICRO                   (1000u)
#endif

#ifndef SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO
#define SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO             (100)
#endif

class SystemTimer {
    struct TimerTaskParams;

    static portMUX_TYPE spinlock;
    static bool initialized;

    typedef std::priority_queue<TimerTaskParams, std::vector<TimerTaskParams>, std::greater<>> PriorityQueue;
    static PriorityQueue timers;

public:
    typedef std::function<void()> CallbackType;
    SystemTimer() = delete;

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

    static bool has_pending_task();

    // Actually it's ~ 53 bits, but it doesn't really matter...
    static uint64_t millis64() { return esp_timer_get_time() / 1000; }
};
