#include "system_timer.h"

bool SystemTimer::task_running = false;
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

    if (!task_running) {
        if (!start_task()) return false;
        task_running = true;
    }

    timers.push({.timeout_at = millis64() + timeout_ms, .callback = std::move(callback)});

    portEXIT_CRITICAL(&spinlock);

    VERBOSE(D_PRINTF("SystemTimer: Add new task. Total: %i\r\n", timers.size()));

    return true;
}

bool SystemTimer::start_task() {
    auto ret = xTaskCreatePinnedToCore(timer_task, "TimerCbTask", 4096, nullptr, 1, nullptr, xPortGetCoreID());
    if (ret != pdPASS) {
        D_PRINTF("SystemTimer: failed to start task: %x\r\n", pdPASS);
        return false;
    }

    return true;
}

[[noreturn]] void SystemTimer::timer_task(void *) {
    while (true) {
        portENTER_CRITICAL(&spinlock);

        auto has_more = false;
        if (timers.empty() || timers.top().timeout_at > millis64()) {
            portEXIT_CRITICAL(&spinlock);
        } else {
            TimerTaskParams value = std::move(const_cast<TimerTaskParams &>(timers.top()));
            timers.pop();

            VERBOSE(D_PRINTF("SystemTimer: remove task. Total: %i\r\n", timers.size()));

            has_more = !timers.empty();
            portEXIT_CRITICAL(&spinlock);

            VERBOSE(D_PRINTF("SystemTimer: triggered at %llu (late for %llu ms)\r\n", value.timeout_at, millis64() - value.timeout_at));

            value.callback();
        }

        if (!has_more) ::delay(1);
    }

    vTaskDelete(nullptr);
}
