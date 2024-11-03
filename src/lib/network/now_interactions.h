#pragma once

#include "async_now.h"

struct __attribute__((__packed__)) EspNowInteractionPacketHeader {
    uint8_t id;
    uint8_t index;
    uint8_t count;
    uint8_t size;
};

struct SendResponse {
    uint8_t id;
    bool ok;
};

constexpr uint8_t ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH = sizeof(EspNowInteractionPacketHeader);
constexpr uint8_t ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH = ESP_NOW_MAX_DATA_LEN - ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH;
constexpr uint16_t ESP_NOW_INTERACTION_MAX_DATA_LENGTH = 0xff * ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH;

class EspNowInteraction {
    static EspNowInteraction _instance;

    bool _initialized = false;
    AsyncEspNow &_async_now = AsyncEspNow::instance();
    uint8_t _id = 0;

    std::unordered_map<uint8_t, std::shared_ptr<Promise<EspNowPacket>>> _requests;

    EspNowInteraction() = default;

public:
    static EspNowInteraction &instance() { return _instance; }

    EspNowInteraction(const EspNowInteraction &) = delete;
    EspNowInteraction &operator=(EspNowInteraction const &) = delete;

    bool begin();

    Future<SendResponse> send(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);
    Future<EspNowPacket> request(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

private:
    Future<SendResponse> _send_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);
    Future<EspNowPacket> _request_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    void _on_packet_received(EspNowPacket packet);
};
