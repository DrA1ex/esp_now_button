#pragma once

#include <lib/network/now_io.h>

#include "base/async_handler.h"
#include "constants.h"

class DiscoveryHandler : public AsyncHandlerBase {
public:
    void discover(unsigned long timeout = DISCOVERY_TIMEOUT);

    [[nodiscard]] const uint8_t *hub_mac_addr() const;
    [[nodiscard]] uint8_t hub_channel() const;

private:
    uint8_t _hub_mac[6] {};
    uint8_t _channel = 0;
};


inline void DiscoveryHandler::discover(unsigned long timeout) {
    _start([=] {
        return NowIo::instance()
               .discover_hub(_hub_mac)
               .then<void>([=](auto &f) { _channel = f.result(); });
    }, timeout);
}

inline const uint8_t *DiscoveryHandler::hub_mac_addr() const { return state() == State::SUCCESS ? _hub_mac : nullptr; }
inline uint8_t DiscoveryHandler::hub_channel() const { return state() == State::SUCCESS ? _channel : 0xff; }
