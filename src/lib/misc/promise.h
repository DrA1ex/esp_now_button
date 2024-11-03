#pragma once

#include <Arduino.h>

#include <memory>
#include <vector>

#include "future.h"
#include "../debug.h"

class FutureBase;
class PromiseBase;
template<typename T> class Future;
template<typename T> class Promise;

#define xIsInISR() (xPortInIsrContext() || xPortInterruptedFromISRContext())

class PromiseBase {
    volatile bool _finished = false;
    volatile bool _success = false;

    std::vector<FutureFinishedCb> _on_finished_callbacks;

#ifdef DEBUG
    int _initial_core_id = xPortGetCoreID();
#endif

public:
    PromiseBase(const PromiseBase &) = delete;
    PromiseBase &operator=(PromiseBase const &) = delete;
    virtual ~PromiseBase() = default;

    [[nodiscard]] virtual bool has_result() const { return false; }

    [[nodiscard]] bool finished() const { return _finished; }
    [[nodiscard]] bool success() const { return _success; }

    [[nodiscard]] bool wait(unsigned long timeout = 0, unsigned long delay_interval = 1) const;
    void on_finished(FutureFinishedCb callback);

    static Future<void> all(const std::vector<FutureBase> &collection);
    static Future<void> any(const std::vector<FutureBase> &collection);

protected:
    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

    PromiseBase() = default;
    PromiseBase(PromiseBase &&) = default;

    void set_success();
    void set_error();

private:
    void _on_promise_finished();
};

template<typename T>
class Promise final : public PromiseBase {
    T _result {};

public:
    Promise() = default;
    Promise(Promise &&) = default;

    [[nodiscard]] bool has_result() const override { return true; }

    [[nodiscard]] T result() const;

    void set_success(T value);

    using PromiseBase::set_error;

    static std::shared_ptr<Promise> create() { return std::make_shared<Promise>(); }
};

template<>
class Promise<void> final : public PromiseBase {
public:
    Promise() = default;
    Promise(Promise &&) = default;

    using PromiseBase::set_success;
    using PromiseBase::set_error;

    static std::shared_ptr<Promise> create() { return std::make_shared<Promise>(); }
};


template<typename T>
T Promise<T>::result() const {
    if (finished() && success()) return _result;

    D_PRINT("Trying to get value of unfinished or unsuccessful promise");
    Serial.flush();

    abort();
}

template<typename T>
void Promise<T>::set_success(T value) {
    portENTER_CRITICAL(&spinlock);

    bool already_finished = finished();
    if (!already_finished) {
        _result = std::move(value);
    }

    portEXIT_CRITICAL(&spinlock);

    if (!already_finished) {
        PromiseBase::set_success();
    }
}
