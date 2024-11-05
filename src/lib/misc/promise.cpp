#include "promise.h"
#include <Arduino.h>

void PromiseBase::set_success() {
    portENTER_CRITICAL(&spinlock);

    bool already_finished = _finished;
    if (!already_finished) {
        _finished = true;
        _success = true;
    }

    portEXIT_CRITICAL(&spinlock);

    if (!already_finished) {
        _on_promise_finished();
    }
}

void PromiseBase::set_error() {
    portENTER_CRITICAL(&spinlock);

    bool already_finished = _finished;
    if (!already_finished) {
        _finished = true;
        _success = false;
    }

    portEXIT_CRITICAL(&spinlock);

    if (!already_finished) {
        _on_promise_finished();
    }
}

void PromiseBase::_on_promise_finished() {
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


bool PromiseBase::wait(unsigned long timeout, unsigned long delay_interval) const {
    if (_finished) return true;

    VERBOSE(D_PRINTF("Promise (%p): waiting, timeout: %lu\r\n", this, timeout));

    auto start = millis();
    while (!_finished && (timeout == 0 || millis() - start < timeout)) {
        delay(delay_interval);
    }

    VERBOSE(D_PRINTF("Promise (%p): Finished with status: %s. Elapsed: %lu\r\n", this, finished() ? "Done" : "Timeout", millis() - start));

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

    portEXIT_CRITICAL(&spinlock);

    if (finished) {
        VERBOSE(D_PRINTF("Promise (%p): Set on_finished callback for already finished promise\r\n", this));
        callback(_success);
    }
}

Future<void> PromiseBase::all(const std::vector<Future<void>> &collection) {
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

    auto finished_cb = [count_left = std::move(count_left), result_promise](bool success) {
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

Future<void> PromiseBase::any(const std::vector<Future<void>> &collection) {
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
