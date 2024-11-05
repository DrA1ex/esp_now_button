#pragma once

#include "base/async_now_interactions.h"

enum class SpecialPacketTypes: uint8_t {
    PING      = 0xf0,
    DISCOVERY = 0xf1,
};

struct __attribute__((__packed__)) PacketHeader {
    uint8_t type;
    uint8_t count;
};

class NowIo {
    static NowIo _instance;

    AsyncEspNowInteraction &_interaction = AsyncEspNowInteraction::instance();

public:
    static NowIo &instance() { return _instance; }

    NowIo(const NowIo &) = delete;
    NowIo &operator=(NowIo const &) = delete;


    bool begin();

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send_packet(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items);
    template<typename T, size_t Count, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send_packet(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]);
    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> send_packet(const uint8_t *mac_addr, uint8_t type, const T &item);
    Future<void> send_packet(const uint8_t *mac_addr, uint8_t type);
    Future<void> send_packet(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<EspNowPacket> request_packet(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items);
    template<typename T, size_t Count, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<void> request_packet(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]);
    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && std::is_standard_layout_v<T>>>
    Future<EspNowPacket> request_packet(const uint8_t *mac_addr, uint8_t type, const T &item);
    Future<EspNowPacket> request_packet(const uint8_t *mac_addr, uint8_t type);
    Future<EspNowPacket> request_packet(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    Future<uint8_t> discover_hub(uint8_t *out_mac_addr);

private:
    NowIo() = default;

    void _fill_packet_data(uint8_t *out_packet, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size);

    Future<uint8_t> _discover_hub_channel(uint8_t channel, uint8_t *out_mac_addr);
};

template<typename T, typename>
Future<void> NowIo::send_packet(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items) {
    return send_packet(mac_addr, type, items.size(), (uint8_t *) items.data(), sizeof(T) * items.size());
}

template<typename T, size_t Count, typename> Future<void>
NowIo::send_packet(const uint8_t *mac_addr, uint8_t type, const T (&items)[Count]) {
    return send_packet(mac_addr, type, Count, (uint8_t *) items, sizeof(T) * Count);
}

template<typename T, typename>
Future<void> NowIo::send_packet(const uint8_t *mac_addr, uint8_t type, const T &item) {
    return send_packet(mac_addr, type, 1, (uint8_t *) &item, sizeof(T));
}

template<typename T, typename>
Future<EspNowPacket> NowIo::request_packet(const uint8_t *mac_addr, uint8_t type, const std::vector<T> &items) {
    return request_packet(mac_addr, type, items.size(), (uint8_t *) items.data(), sizeof(T) * items.size());
}

template<typename T, size_t Count, typename> Future<void>
NowIo::request_packet(const uint8_t *mac_addr, uint8_t type, const T(&items)[Count]) {
    return request_packet(mac_addr, type, Count, (uint8_t *) items, sizeof(T) * Count);
}

template<typename T, typename>
Future<EspNowPacket> NowIo::request_packet(const uint8_t *mac_addr, uint8_t type, const T &item) {
    return request_packet(mac_addr, type, 1, (uint8_t *) &item, sizeof(T));
}
