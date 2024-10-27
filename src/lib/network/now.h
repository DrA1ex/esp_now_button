#pragma once

#include <cstdint>
#include <vector>

#include <esp_now.h>
#include <WiFi.h>

#include "../debug.h"
#include "../type.h"


struct __attribute__((__packed__)) EspNowPacketHeader {
    uint8_t id;
    uint8_t index;
    uint8_t count;
    uint8_t size;
};

struct EspNowReceivingMeta {
    unsigned long created_at;

    uint8_t id;
    uint8_t index;
    uint8_t count;

    int size;
    uint8_t *data;
};

constexpr int ESP_NOW_PACKET_HEADER_LENGTH = sizeof(EspNowPacketHeader);
constexpr int ESP_NOW_MAX_PACKET_DATA_LENGTH = ESP_NOW_MAX_DATA_LEN - ESP_NOW_PACKET_HEADER_LENGTH;
constexpr int ESP_NOW_MAX_DATA_LENGTH = 0xff * ESP_NOW_MAX_PACKET_DATA_LENGTH;

typedef std::function<void(const uint8_t *mac_addr, const uint8_t *data, int size)> EspNowOnMessageCb;
typedef std::function<void(const uint8_t *mac_addr, int status)> EspNowOnSentCb;

class EspNow {
    static EspNow static_instance;

    RTC_DATA_ATTR static bool wifi_channel_found;
    RTC_DATA_ATTR static uint8_t wifi_channel;

    EspNow() = default;

    bool _initialized = false;
    uint8_t _id = 0;

    std::vector<esp_now_peer_info> _peers;
    std::unordered_map<uint8_t, EspNowReceivingMeta> _receiving_data;

    EspNowOnMessageCb _on_message_cb = nullptr;
    EspNowOnSentCb _on_sent_cb = nullptr;

public:
    static EspNow &instance() { return static_instance; }

    EspNow(const EspNow &) = delete;
    EspNow &operator=(EspNow const &) = delete;

    bool begin();

    bool configure_channel(const uint8_t *mac_addr);

    template<typename T, typename = std::enable_if_t<!std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    bool send(const uint8_t *mac_addr, const T &value);
    bool send(const uint8_t *mac_addr, const char *str);
    bool send(const uint8_t *mac_addr, const uint8_t *data, int size);

    bool ping(const uint8_t *mac_addr);

    inline void set_on_message(EspNowOnMessageCb &&fn) { _on_message_cb = fn; }
    inline void set_on_sent(EspNowOnSentCb &&fn) { _on_sent_cb = fn; }

    inline void print_mac() const { D_PRINTF("Mac: %s\r\n", WiFi.macAddress().c_str()); }

    template<typename T, uint8_t Count, typename = std::enable_if_t<
            !std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    bool send_packet(const uint8_t *mac_addr, PacketType type, const T &value);

    template<typename T, uint8_t Count, typename = std::enable_if_t<
            !std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    bool send_packet(const uint8_t *mac_addr, PacketType type, const T (&values)[Count]);

    template<typename T, typename = std::enable_if_t<
            !std::is_pointer_v<T> && (std::is_scalar_v<T> || std::is_standard_layout_v<T>)>>
    bool send_packet(const uint8_t *mac_addr, PacketType type, uint8_t count, const T *values);

protected:
    void register_peer(const uint8_t *mac_addr);

    static void received(const uint8_t *mac_addr, const uint8_t *data, int data_len);
    static void sent(const uint8_t *mac_addr, esp_now_send_status_t status);

private:
    void _process_packet(const uint8_t *mac_addr, const EspNowPacketHeader &packet, const uint8_t *data, uint8_t data_size);
    void _erase_receiving_data(uint8_t id);

    void _change_channel(uint8_t channel);
    bool _check_channel(const uint8_t *mac_addr, uint8_t channel);
    uint8_t _find_channel(const uint8_t *mac_addr);
};

template<typename T, typename>
bool EspNow::send(const uint8_t *mac_addr, const T &value) {
    return send(mac_addr, (uint8_t *) &value, sizeof(value));
}

template<typename T, uint8_t Count, typename>
bool EspNow::send_packet(const uint8_t *mac_addr, PacketType type, const T &value) {
    return send_packet(mac_addr, type, 1, &value);
}

template<typename T, uint8_t Count, typename>
bool EspNow::send_packet(const uint8_t *mac_addr, PacketType type, const T (&values)[Count]) {
    return send_packet(mac_addr, type, Count, values);
}

template<typename T, typename>
bool EspNow::send_packet(const uint8_t *mac_addr, PacketType type, uint8_t count, const T *values) {
    const auto header_size = sizeof(PacketHeader);
    const auto values_size = count * sizeof(T);

    uint8_t data[header_size + values_size];

    auto *header = (PacketHeader *) data;
    header->type = type;
    header->count = count;

    memcpy(data + header_size, values, values_size);

    return send(mac_addr, data, sizeof(data));
}