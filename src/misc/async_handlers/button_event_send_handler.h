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

        auto send_fn = [=, &events] {
            return NowIo::instance().send(mac_addr, (uint8_t) PacketType::BUTTON, events)
                                    .with_timeout(timeout);
        };

        auto retry_left = std::make_shared<int>(SEND_RETRY_COUNT);
        return PromiseBase::sequential<void>(send_fn(),
            [=](auto &f) {
                if (f.success()) return false;

                if ((*retry_left)-- > 0) {
                    D_PRINT("ButtonEventSendHandler: Data sending failed. Retrying...");
                    return true;
                }

                D_PRINT("ButtonEventSendHandler: Data sending failed. No Retry attempts left");
                return false;
            },
            [send_fn = std::move(send_fn)](auto) {
                return SystemTimer::delay(SEND_RETRY_DELAY)
                        .then<void>([=](auto) { return send_fn(); });
            });
    }, 0);
}
