#pragma once

#include <Arduino.h>

class Led {
    uint16_t _max_brightness = 0xff;
    unsigned long _blink_active_duration = 60ul;
    unsigned long _blink_wait_duration = 140ul;
    unsigned long _blink_repeat_interval = 3000ul;


    uint8_t _pin;
    uint16_t _brightness = _max_brightness;

    bool _active = false;

    bool _continuously = false;
    uint8_t _blink_count = 0;
    uint8_t _blink_count_left = 0;
    unsigned long _flash_duration = 0;

    unsigned long _start_time = 0;

    void _refresh_led(bool active);

    bool _initialized = false;

public:
    explicit Led(uint8_t pin);

    void begin();

    void set_brightness(uint16_t value) { _brightness = value; }

    void blink(uint8_t count = 1, bool continuously = false);
    void flash(unsigned long duration = 0);

    void turn_off();

    void tick();

    [[nodiscard]] inline bool initialized() const { return _initialized; }
    [[nodiscard]] inline bool active() const { return _active; }
    [[nodiscard]] inline uint8_t blink_count() const { return _blink_count; }
    [[nodiscard]] inline unsigned long flash_duration() const { return _flash_duration; }


    [[nodiscard]] inline uint16_t max_brightness() const { return _max_brightness; }
    inline void set_max_brightness(uint16_t value) { _max_brightness = value; }
    [[nodiscard]] inline unsigned long blink_active_duration() const { return _blink_active_duration; }
    inline void set_blink_active_duration(unsigned long duration) { _blink_active_duration = duration; }
    [[nodiscard]] inline unsigned long blink_wait_duration() const { return _blink_wait_duration; }
    inline void set_blink_wait_duration(unsigned long duration) { _blink_wait_duration = duration; }
    [[nodiscard]] inline unsigned long blink_repeat_interval() const { return _blink_repeat_interval; }
    inline void set_blink_repeat_interval(unsigned long interval) { _blink_repeat_interval = interval; }
};
