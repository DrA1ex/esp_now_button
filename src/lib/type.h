#pragma once

#include <cstdint>

enum class PacketType : uint8_t {
    BUTTON = 0,

    PING = 0xf0
};

enum class ButtonEventType : uint8_t {
    CLICKED = 0,
    HOLD = 1,
    RELEASED = 2
};

struct __attribute__((__packed__)) ButtonEvent {
    ButtonEventType event_type;
    uint8_t click_count;
};

struct __attribute__((__packed__)) PacketHeader {
    PacketType type;
    uint8_t count;
};
