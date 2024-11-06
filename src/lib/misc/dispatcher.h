#pragma once

#include <Arduino.h>
#include <functional>
#include <queue>

#ifndef DISPATCHER_STACK_SIZE
#define DISPATCHER_STACK_SIZE                               (4096u)
#endif

#ifndef DISPATCHER_TASK_PRIORITY
#define DISPATCHER_TASK_PRIORITY                            (1u)
#endif

#ifndef DISPATCHER_TASK_RUNNING_TIMEOUT_MICRO
#define DISPATCHER_TASK_RUNNING_TIMEOUT_MICRO               (100)
#endif

class Dispatcher {
    static bool initialized;
    static portMUX_TYPE spinlock;
    static TaskHandle_t task_handle;

public:
    typedef std::function<void()> DispatchFn;
    Dispatcher() = delete;

    static bool begin();
    static bool dispatch(DispatchFn fn);

private:
    static std::queue<DispatchFn> dispatched;

    static bool notify_from_isr();
    static bool notify();

    static void dispatcher_task(void *);
};
