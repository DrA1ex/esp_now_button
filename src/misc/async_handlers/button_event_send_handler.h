#pragma once

#include <lib/network/now_io.h>

#include "base/async_handler.h"
#include "constants.h"
#include "type.h"


class ButtonEventSendHandler : public AsyncHandlerBase {
public:
    void send(const uint8_t *mac_addr, const Vector<ButtonEvent> &events, unsigned long timeout = SEND_TIMEOUT);
};


inline void ButtonEventSendHandler::send(const uint8_t *mac_addr, const Vector<ButtonEvent> &events, unsigned long timeout) {
    _start([&] {
        D_PRINT("ButtonEventSender: Sending events:");
        for (int i = 0; i < events.size(); ++i) {
            D_PRINTF("\t- Button #%i: Type: %i, Count %i\r\n", i, events[i].event_type, events[i].click_count);
        }

        return NowIo::instance().send(mac_addr, (uint8_t) PacketType::BUTTON, events);
    }, timeout);
}
