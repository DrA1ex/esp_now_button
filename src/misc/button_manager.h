#pragma once

#include <lib/misc/button.h>
#include <lib/misc/vector.h>

#include "constants.h"
#include "type.h"

template<uint8_t Size>
class ButtonManager {
    bool _initialized = false;

    std::array<uint8_t, Size> _pins;
    Vector<Button> _buttons;
    Vector<ButtonEvent> _events;

public:
    explicit ButtonManager(const std::array<uint8_t, Size> &pins): _pins(pins) {}

    void begin();
    void end();

    void tick();

    [[nodiscard]] bool idle() const;
    [[nodiscard]] bool holding() const;
    [[nodiscard]] bool active() const;
    [[nodiscard]] bool empty() const;

    [[nodiscard]] const Vector<ButtonEvent> &events();
};

template<uint8_t Size>
void ButtonManager<Size>::begin() {
    if (_initialized) return;

    _events.resize(Size);
    _buttons.reserve(Size);
    for (int i = 0; i < Size; ++i) {
        auto &button = _buttons.emplace(_pins[i], BTN_HIGH_STATE, true);
        button.begin(BTN_MODE);
    }

    _initialized = true;
}

template<uint8_t Size>
void ButtonManager<Size>::end() {
    if (!_initialized) return;

    _buttons.clear();
    _events.clear();
    _initialized = false;
}

template<uint8_t Size>
void ButtonManager<Size>::tick() {
    if (!_initialized) return;

    for (int i = 0; i < Size; ++i) _buttons[i].handle();
}

template<uint8_t Size>
bool ButtonManager<Size>::idle() const {
    return std::all_of(_buttons.begin(), _buttons.end(), [](auto &b) { return b.idle(); });
}

template<uint8_t Size>
bool ButtonManager<Size>::holding() const {
    return std::any_of(_buttons.begin(), _buttons.end(), [](auto &b) {
        return !b.idle() && b.last_state().hold;
    });
}

template<uint8_t Size>
bool ButtonManager<Size>::active() const {
    return std::all_of(_buttons.begin(), _buttons.end(), [](auto &b) { return !b.idle(); });
}

template<uint8_t Size> bool ButtonManager<Size>::empty() const {
    return std::all_of(_buttons.begin(), _buttons.end(), [](auto &b) { return b.last_state().click_count == 0; });
}

template<uint8_t Size>
const Vector<ButtonEvent> &ButtonManager<Size>::events() {
    for (int i = 0; i < Size; ++i) {
        auto &button = _buttons[i];
        auto &btn_state = button.last_state();
        _events[i] = {
            .event_type = btn_state.hold
                              ? (button.idle() ? ButtonEventType::RELEASED : ButtonEventType::HOLD)
                              : ButtonEventType::CLICKED,
            .click_count = btn_state.click_count
        };
    }

    return _events;
}
