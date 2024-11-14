#include "button.h"

#include <esp_sleep.h>

#include "../debug.h"

Button::Button(uint8_t pin, bool high_state, bool used_for_wakeup)
    : _pin(pin), _high_state(high_state), _used_for_wakeup(used_for_wakeup) {}

Button::~Button() noexcept {
    end();
}

void Button::begin(uint8_t mode) {
    if (_initialized) return;

    pinMode(_pin, mode);

#ifdef SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP
    if (_used_for_wakeup) {
        auto wakeup_status = esp_sleep_get_gpio_wakeup_status();
        bool waked_up = wakeup_status & (1 << _pin);
        if (!_read() && waked_up) {
            _last_impulse_time = millis();
            _click_count = 1;
        }
    }
#endif

    _last_interrupt_state = _read();
    attachInterruptArg(digitalPinToInterrupt(_pin), _handle_interrupt_change_static, this, CHANGE);

    D_PRINTF("Button(%u): Setup button interruption\r\n", _pin);
    _initialized = true;
}

void Button::end() noexcept {
    if (!_initialized) return;

    detachInterrupt(digitalPinToInterrupt(_pin));
    pinMode(_pin, INPUT);

    _click_count = 0;
    _hold = false;
    _initialized = false;

    D_PRINTF("Button(%u): Detach button interruption\r\n", _pin);
}

bool Button::_read() const {
    return digitalRead(_pin) ^ !_high_state;
}

void Button::_handle_interrupt_change_static(void *arg) {
    auto self = (Button *) arg;
    self->_handle_interrupt_change();
}

void Button::_handle_interrupt_change() {
    auto delta = millis() - _last_impulse_time;
    _last_impulse_time = millis();

    if (delta < BTN_SILENCE_INTERVAL) {
        VERBOSE(D_PRINTF("Button(%u): Filtering noise\r\n", _pin));
        return;
    }

    bool state = _read();
    if (state == _last_interrupt_state) {
        VERBOSE(D_PRINTF("Button(%u): Interrupted with the same state (%i). Inverting\r\n", _pin, state));
        state = !state;
    }

    _last_interrupt_state = state;
    if (state) {
        _handle_rising_interrupt(delta);
    } else {
        _handle_falling_interrupt(delta);
    }
}


void Button::_handle_rising_interrupt(unsigned long delta) {
    VERBOSE(D_PRINTF("Button(%u): Interruption RISING\r\n", _pin));
    if ((_click_count || _hold) && delta > BTN_RESET_INTERVAL) {
        VERBOSE(D_PRINTF("Button(%u): Start Over. Forget to call Button::handle() ?\r\n", _pin));
        _hold = false;
        _click_count = 0;
    }
}

void Button::_handle_falling_interrupt(unsigned long delta) {
    VERBOSE(D_PRINTF("Button(%u): Interruption FALLING\r\n", _pin));
    if (!_hold) {
        VERBOSE(D_PRINTF("Button(%u): Interruption Click\r\n", _pin));
        _click_count++;
    }
}


void Button::handle() {
    if (!_initialized) return;

    unsigned long delta = millis() - _last_impulse_time;

    const bool state = _read();
    if (!_hold && state && delta >= BTN_HOLD_INTERVAL) {
        VERBOSE(D_PRINTF("Button(%u): Set Hold\r\n", _pin));
        _hold = true;
        _click_count++;
    } else if (_hold && !state) {
        D_PRINTF("Button(%u): Hold Release\r\n", _pin);

        if (_hold_release_handler != nullptr) {
            _hold_release_handler(_click_count);
        }

        _hold = false;
        _click_count = 0;
        _last_interrupt_state = false;
    }

    if (_hold) {
        auto hold_call_delta = millis() - _last_button_hold_call_time;
        if (hold_call_delta >= BTN_HOLD_CALL_INTERVAL) {
            D_PRINTF("Button(%u): Hold #%i\r\n", _pin, _click_count);

            _last_state.click_count = _click_count;
            _last_state.hold = true;
            _last_state.timestamp = millis();

            if (_hold_handler != nullptr) {
                _hold_handler(_click_count);
            }

            _last_button_hold_call_time = millis();
        }
    } else if (_click_count && delta > BTN_PRESS_WAIT_INTERVAL) {
        D_PRINTF("Button(%u): Click count %i\r\n", _pin, _click_count);

        _last_state.click_count = _click_count;
        _last_state.hold = false;
        _last_state.timestamp = millis();

        if (_click_handler != nullptr) {
            _click_handler(_click_count);
        }

        _click_count = 0;
        _last_interrupt_state = false;
    }
}
