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
};

constexpr uint8_t ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH = sizeof(EspNowInteractionPacketHeader);
constexpr uint8_t ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH = ESP_NOW_MAX_DATA_LEN - ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH;
constexpr uint16_t ESP_NOW_INTERACTION_MAX_DATA_LENGTH = 0xff * ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH;

class AsyncEspNowInteraction {
    static AsyncEspNowInteraction _instance;

    bool _initialized = false;
    AsyncEspNow &_async_now = AsyncEspNow::instance();
    uint8_t _id = 0;

    std::unordered_map<uint8_t, std::shared_ptr<Promise<EspNowPacket>>> _requests;

    AsyncEspNowInteraction() = default;

public:
    static const uint8_t BROADCAST_MAC[6];
    static AsyncEspNowInteraction &instance() { return _instance; }

    AsyncEspNowInteraction(const AsyncEspNowInteraction &) = delete;
    AsyncEspNowInteraction &operator=(AsyncEspNowInteraction const &) = delete;

    bool begin();

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    Future<SendResponse> send(const uint8_t *mac_addr, const T &value);
    Future<SendResponse> send(const uint8_t *mac_addr, const char *str);
    Future<SendResponse> send(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    Future<EspNowPacket> request(const uint8_t *mac_addr, const T &value);
    Future<EspNowPacket> request(const uint8_t *mac_addr, const char *str);
    Future<EspNowPacket> request(const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    Future<uint8_t> discover_peer_channel(const uint8_t *mac_addr);

    static void print_mac() { D_PRINTF("Mac: %s\r\n", WiFi.macAddress().c_str()); }

private:
    Future<SendResponse> _send_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);
    Future<EspNowPacket> _request_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size);

    Future<uint8_t> _configure_peer_channel(const uint8_t *mac_addr, uint8_t channel);

    void _on_packet_received(EspNowPacket packet);
};

template<typename T, typename>
Future<SendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const T &value) {
    return send(mac_addr, &value, sizeof(T));
}

template<typename T, typename>
Future<EspNowPacket> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const T &value) {
    return request(mac_addr, &value, sizeof(T));
}
