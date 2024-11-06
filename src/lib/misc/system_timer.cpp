#include "system_timer.h"

bool SystemTimer::initialized = false;
portMUX_TYPE SystemTimer::spinlock = portMUX_INITIALIZER_UNLOCKED;
SystemTimer::PriorityQueue SystemTimer::timers {};


Future<void> SystemTimer::delay(unsigned long timeout_ms) {
    auto promise = Promise<void>::create();
    auto callback = [=] {
        promise->set_success();
    };

    if (!set_timeout(timeout_ms, std::move(callback))) {
        promise->set_error();
    }

    return Future {promise};
}

bool SystemTimer::set_timeout(unsigned long timeout_ms, CallbackType callback) {
    if (callback == nullptr) return false;

    portENTER_CRITICAL(&spinlock);

    if (!initialized) {
        if (!start_task()) return false;
        initialized = true;
    }

    timers.push({.timeout_at = millis64() + timeout_ms, .callback = std::move(callback)});

    portEXIT_CRITICAL(&spinlock);

    VERBOSE(D_PRINTF("SystemTimer: Add new task. Total: %i\r\n", timers.size()));

    return true;
}

bool SystemTimer::start_task() {
    auto ret = xTaskCreatePinnedToCore(timer_task, "TimerCbTask",
        SYSTEM_TIMER_STACK_SIZE, nullptr, SYSTEM_TIMER_TASK_PRIORITY, nullptr, xPortGetCoreID());

    if (ret != pdPASS) {
        D_PRINTF("SystemTimer: Failed to start task: %x\r\n", pdPASS);
        return false;
    }

    return true;
}

[[noreturn]] void SystemTimer::timer_task(void *) {
    while (true) {
        auto begin_micros = esp_timer_get_time();

        bool has_pending;
        do {
            portENTER_CRITICAL(&spinlock);
            has_pending = has_pending_task();

            if (has_pending) {
                TimerTaskParams value = std::move(const_cast<TimerTaskParams &>(timers.top()));
                timers.pop();
                VERBOSE(D_PRINTF("SystemTimer: Remove task. Left: %lu\r\n", timers.size()));

                has_pending = has_pending_task();
                portEXIT_CRITICAL(&spinlock);

                VERBOSE(D_PRINTF("SystemTimer: Triggered at %llu (late for %llu ms)\r\n", value.timeout_at, millis64() - value.timeout_at));
                value.callback();
            } else {
                portEXIT_CRITICAL(&spinlock);
            }

            if (has_pending && esp_timer_get_time() - begin_micros > SYSTEM_TIMER_TASK_RUNNING_TIMEOUT_MICRO) {
                vTaskDelay(0);
                VERBOSE(D_PRINT("SystemTimer: Too long task execution. Wait before continue"));
                begin_micros = esp_timer_get_time();
            }
        } while (has_pending);

        delayMicroseconds(SYSTEM_TIMER_DELAY_INTERVAL_MICRO);
    }

    vTaskDelete(nullptr);
}

bool SystemTimer::has_pending_task() {
    return !timers.empty() && timers.top().timeout_at < millis64();
}
