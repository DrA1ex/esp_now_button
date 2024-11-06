#include "dispatcher.h"

#include <lib/debug.h>

#define xIsInISR() (xPortInIsrContext() || xPortInterruptedFromISRContext())

bool Dispatcher::initialized = false;
portMUX_TYPE Dispatcher::spinlock = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t Dispatcher::task_handle = nullptr;
std::queue<Dispatcher::DispatchFn> Dispatcher::dispatched {};

bool Dispatcher::begin() {
    if (xIsInISR()) {
        D_PRINT("Dispatcher: Initialization in ISR context is forbidden");
        return false;
    }

    portENTER_CRITICAL(&spinlock);

    if (!initialized) {
        auto ret = xTaskCreatePinnedToCore(dispatcher_task, "DispatcherTask",
            DISPATCHER_STACK_SIZE, nullptr, DISPATCHER_TASK_PRIORITY, &task_handle, xPortGetCoreID());

        if (ret != pdPASS) {
            D_PRINTF("Dispatcher: Failed to start task: %x\r\n", pdPASS);
            return false;
        }

        VERBOSE(D_PRINT("Dispatcher: Initialized"));
        initialized = true;
    }

    portEXIT_CRITICAL(&spinlock);

    return true;
}

bool Dispatcher::dispatch(DispatchFn fn) {
    if (!initialized && !begin()) return false;

    portENTER_CRITICAL(&spinlock);
    dispatched.push(std::move(fn));

    bool success = xIsInISR() ? notify_from_isr() : notify();
    if (!success) {
        D_PRINT("Dispatcher: Failed to notify dispatcher task");
        dispatched.pop();
    }
    portEXIT_CRITICAL(&spinlock);

    return success;
}

bool Dispatcher::notify_from_isr() {
    return xTaskNotifyFromISR(task_handle, 0, eNoAction, nullptr) == pdPASS;
}

bool Dispatcher::notify() {
    return xTaskNotify(task_handle, 0, eNoAction) == pdPASS;
}

[[noreturn]] void Dispatcher::dispatcher_task(void *) {
    while (true) {
        VERBOSE(D_PRINT("Dispatcher: Wait for events..."));
        xTaskNotifyWaitIndexed(0, 0x00, ULONG_MAX, nullptr, portMAX_DELAY);

        auto begin_micros = esp_timer_get_time();

        bool empty;
        do {
            portENTER_CRITICAL(&spinlock);
            empty = dispatched.empty();

            if (!empty) {
                auto cb = std::move(dispatched.front());
                dispatched.pop();

                empty = dispatched.empty();
                portEXIT_CRITICAL(&spinlock);

                VERBOSE(D_PRINTF("Dispatcher: Running dispatched function. Left: %lu\r\n", dispatched.size()));
                cb();
            } else {
                portEXIT_CRITICAL(&spinlock);
            }

            if (!empty && esp_timer_get_time() - begin_micros > DISPATCHER_TASK_RUNNING_TIMEOUT_MICRO) {
                vTaskDelay(0);
                VERBOSE(D_PRINT("Dispatcher: Too long task execution. Wait before continue"));
                begin_micros = esp_timer_get_time();
            }
        } while (!empty);
    }
}
