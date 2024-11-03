#pragma once

#include <memory>
#include <vector>

#include "../debug.h"

class FutureBase;
class PromiseBase;
template<typename T> class Future;
template<typename T> class Promise;

typedef std::function<void(bool success)> FutureFinishedCb;

#define xIsInISR() (xPortInIsrContext() || xPortInterruptedFromISRContext())

class FutureBase {
protected:
    const std::shared_ptr<PromiseBase> promise;

public:
    explicit FutureBase(const std::shared_ptr<PromiseBase> &promise) : promise(promise) {}
    virtual ~FutureBase() = default;

    [[nodiscard]] virtual inline bool has_result() const;

    [[nodiscard]] inline bool finished() const;
    [[nodiscard]] inline bool success() const;

    [[nodiscard]] inline bool wait(unsigned long timeout = 0, unsigned long delay_interval = 1) const;
    inline void on_finished(FutureFinishedCb callback) const;

    template<typename T>
    Future<T> then(std::function<Future<T>(const FutureBase &)> fn) {
        auto chained_promise = Promise<T>::create();

        on_finished([this, fn = std::move(fn), chained_promise](bool success) {
            if (success) {
                auto ret_promise = fn(*this);
                ret_promise.on_finished([ret_promise, chained_promise](bool success) {
                    if (success) chained_promise->set_success(ret_promise.result());
                    else chained_promise->set_error();
                });
            } else {
                chained_promise->set_error();
            }
        });

        return chained_promise;
    }

    template<typename T>
    Future<T> then(std::function<T(const FutureBase &)> fn) {
        auto chained_promise = Promise<T>::create();

        on_finished([this, fn = std::move(fn), chained_promise](bool success) {
            if (success) chained_promise->set_success(fn(*this));
            else chained_promise.set_error();
        });

        return chained_promise;
    }
};

template<typename T>
class Future final : public FutureBase {
public:
    Future(const std::shared_ptr<Promise<T>> &promise); // NOLINT(*-explicit-constructor)

    [[nodiscard]] T result() const;

    static Future successful(T value);
    static Future errored();
};

template<>
class Future<void> final : public FutureBase {
public:
    inline Future(const std::shared_ptr<Promise<void>> &promise); // NOLINT(*-explicit-constructor)
    inline Future(const std::shared_ptr<PromiseBase> &promise); // NOLINT(*-explicit-constructor)
    inline Future(const FutureBase &future); // NOLINT(*-explicit-constructor)

    static inline Future successful();
    static inline Future errored();
};

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

    [[nodiscard]] inline bool wait(unsigned long timeout = 0, unsigned long delay_interval = 1) const;
    inline void on_finished(FutureFinishedCb callback);

    static inline Future<void> all(const std::vector<FutureBase> &collection);
    static inline Future<void> any(const std::vector<FutureBase> &collection);

protected:
    portMUX_TYPE spinlock = portMUX_INITIALIZER_UNLOCKED;

    PromiseBase() = default;
    PromiseBase(PromiseBase &&) = default;

    void set_success() {
        portENTER_CRITICAL(&spinlock);

        bool already_finished = _finished;
        if (!already_finished) {
            _finished = true;
            _success = true;
        }

        //TODO: Call callback without critical
        if (!already_finished) {
            _on_promise_finished();
        }
        portEXIT_CRITICAL(&spinlock);
    }

    void set_error() {
        portENTER_CRITICAL(&spinlock);

        bool already_finished = _finished;
        if (!already_finished) {
            _finished = true;
            _success = false;
        }

        if (!already_finished) {
            _on_promise_finished();
        }
        portEXIT_CRITICAL(&spinlock);
    }

private:
    void _on_promise_finished() {
#ifdef DEBUG
        if (xIsInISR() || _initial_core_id != xPortGetCoreID()) {
            D_PRINTF("Promise (%p): Finished from different thread or ISR. It may lead to undefined behavior\r\n", this);
        }
#endif

        VERBOSE(D_PRINTF("Promise (%p): Done\r\n", this));
        if (_on_finished_callbacks.empty()) return;

        for (auto &callback: _on_finished_callbacks) callback(_success);
        _on_finished_callbacks.clear();
    }
};

template<typename T>
class Promise final : public PromiseBase {
    T _result {};

public:
    Promise() = default;
    Promise(Promise &&) = default;

    [[nodiscard]] bool has_result() const override { return true; }

    [[nodiscard]] T result() const {
        if (finished() && success()) return _result;

        D_PRINT("Trying to get value of unfinished or unsuccessful promise");
        Serial.flush();

        abort();
    }

    void set_success(T value) {
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

bool FutureBase::has_result() const { return promise->has_result(); }
bool FutureBase::finished() const { return promise->finished(); }
bool FutureBase::success() const { return promise->success(); }

bool FutureBase::wait(unsigned long timeout, unsigned long delay_interval) const { return promise->wait(timeout, delay_interval); }

void FutureBase::on_finished(FutureFinishedCb callback) const { promise->on_finished(std::move(callback)); }

template<typename T>
Future<T>::Future(const std::shared_ptr<Promise<T>> &promise) : FutureBase(promise) {}

template<typename T>
T Future<T>::result() const { return ((const std::shared_ptr<Promise<T>> &) promise)->result(); }

template<typename T>
Future<T> Future<T>::errored() {
    auto promise = Promise<T>::create();
    promise->set_error();
    return promise;
}

template<typename T>
Future<T> Future<T>::successful(T value) {
    auto promise = Promise<T>::create();
    promise->set_success(value);
    return promise;
}

Future<void>::Future(const std::shared_ptr<Promise<void>> &promise) : FutureBase(promise) {}
Future<void>::Future(const std::shared_ptr<PromiseBase> &promise) : FutureBase(promise) {}
Future<void>::Future(const FutureBase &future) : FutureBase(future) {}

Future<void> Future<void>::successful() {
    auto promise = Promise<void>::create();
    promise->set_success();
    return promise;
}

Future<void> Future<void>::errored() {
    auto promise = Promise<void>::create();
    promise->set_error();
    return promise;
}

bool PromiseBase::wait(unsigned long timeout, unsigned long delay_interval) const {
    if (_finished) return true;

    VERBOSE(D_PRINTF("Promise: waiting, timeout: %lu\r\n", timeout));

    auto start = millis();
    while (!_finished && (timeout == 0 || millis() - start < timeout)) {
        delay(delay_interval);
    }

    VERBOSE(D_PRINTF("Promise: Finished with status: %s. Elapsed: %lu\r\n", finished() ? "Done" : "Timeout", millis() - start));

    return _finished;
}

void PromiseBase::on_finished(FutureFinishedCb callback) {
#ifdef DEBUG
    if (xIsInISR() || _initial_core_id != xPortGetCoreID()) {
        D_PRINTF("Promise (%p): Set on_finished callback from different thread  or ISR. It may lead to undefined behavior\r\n", this);
    }
#endif

    portENTER_CRITICAL(&spinlock);

    bool finished = _finished;
    if (!finished) {
        VERBOSE(D_PRINTF("Promise (%p): Add on_finished callback\r\n", this));
        _on_finished_callbacks.push_back(std::move(callback));
    }

    if (finished) {
        VERBOSE(D_PRINTF("Promise (%p): Set on_finished callback for already finished promise\r\n", this));
        callback(_success);
    }

    portEXIT_CRITICAL(&spinlock);
}

Future<void> PromiseBase::all(const std::vector<FutureBase> &collection) {
    if (collection.empty()) return Future<void>::errored();
    if (collection.size() == 1) return collection[0];

    VERBOSE(D_PRINTF("Promise::all(): Start aggregation of %i futures\r\n", collection.size()));

    bool already_finished = true;
    for (const auto &future: collection) {
        if (!future.finished()) { already_finished = false; } else if (!future.success()) {
            VERBOSE(D_PRINTF("Promise::all(): Already failed\r\n"));
            return Future<void>::errored();
        }
    }

    if (already_finished) {
        VERBOSE(D_PRINTF("Promise::all(): Already done\r\n"));
        return Future<void>::successful();
    }

    auto result_promise = Promise<void>::create();
    auto count_left = std::make_shared<std::size_t>(collection.size());

    auto finished_cb = [count_left, result_promise](bool success) {
        VERBOSE(D_PRINTF("Promise::all(): Promise finished, left: %i\r\n", *count_left - 1));
        if (result_promise->finished()) return;

        if (!success) {
            VERBOSE(D_PRINT("Promise::all(): Finished with result: Error"));
            result_promise->set_error();
        } else if (--*count_left == 0) {
            VERBOSE(D_PRINT("Promise::all(): Finished with result: Done"));
            result_promise->set_success();
        }
    };

    for (auto &future: collection) { future.on_finished(finished_cb); }

    return Future {result_promise};
}

Future<void> PromiseBase::any(const std::vector<FutureBase> &collection) {
    if (collection.empty()) return Future<void>::errored();
    if (collection.size() == 1) return collection[0];

    VERBOSE(D_PRINTF("Promise::any(): Start aggregation of %i futures\r\n", collection.size()));

    for (const auto &future: collection) {
        if (future.finished()) {
            VERBOSE(D_PRINTF("Promise::any(): Already finished with result: %s\r\n",
                future.success() ? "Done" : "Error"));

            return future.success()
                       ? Future<void>::successful()
                       : Future<void>::errored();
        }
    }

    auto result_promise = std::make_shared<Promise<void>>();
    auto finished_cb = [result_promise](bool success) {
        if (result_promise->finished()) return;

        VERBOSE(D_PRINTF("Promise::any(): Finished with result: %s\r\n", success ? "Done" : "Error"));

        if (success) result_promise->set_success();
        else result_promise->set_error();
    };

    for (const auto &future: collection) { future.on_finished(finished_cb); }

    return Future<void> {result_promise};
}
