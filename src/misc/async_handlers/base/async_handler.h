#pragma once

#include <lib/misc/future.h>
#include <lib/misc/system_timer.h>

class AsyncHandlerBase {
public:
    enum class State { NOT_STARTED, PENDING, SUCCESS, ERROR, TIMEOUT };

    virtual ~AsyncHandlerBase() = default;

    [[nodiscard]] virtual State state() const { return _state; }

protected:
    AsyncHandlerBase() = default;

    virtual void _start(const std::function<Future<void>()> &future_fn, unsigned long timeout);

private:
    State _state = State::NOT_STARTED;
    Future<void> _future = Future<void>::errored();
};

inline void AsyncHandlerBase::_start(const std::function<Future<void>()> &future_fn, unsigned long timeout) {
    if (_state == State::PENDING) {
        D_PRINT("AsyncHandlerBase: Handler still running. Skipping...");
        return;
    }

    if (timeout > 0) {
        bool timer_ok = SystemTimer::set_timeout(timeout, [&] {
            if (_state == State::PENDING) _state = State::TIMEOUT;
        });

        if (!timer_ok) {
            D_PRINT("AsyncHandlerBase: Unable to set discovery timeout");
            _state = State::ERROR;
            return;
        }
    }

    _state = State::PENDING;
    _future = future_fn();
    _future.on_finished([&](bool success) {
        if (_state == State::PENDING) _state = success ? State::SUCCESS : State::ERROR;
    });
}
