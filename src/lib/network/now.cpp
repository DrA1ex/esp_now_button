#include "now.h"

#include <esp_wifi.h>

EspNow EspNow::static_instance{};

bool EspNow::wifi_channel_found = false;
uint8_t EspNow::wifi_channel = 0;

bool EspNow::begin() {
    if (_initialized) return false;

    if (WiFiClass::getMode() == WIFI_MODE_NULL) WiFiClass::mode(WIFI_MODE_STA);

    auto ret = esp_now_init();
    if (ret != ESP_OK) {
        D_PRINTF("Unable to initialize ESP NOW: %i\r'n", ret);
        return false;
    }

    esp_now_register_recv_cb(EspNow::received);
    esp_now_register_send_cb(EspNow::sent);

    _initialized = true;
    return true;
}

bool EspNow::send(const uint8_t *mac_addr, const uint8_t *data, int size) {
    if (!_initialized || size == 0) return false;
    if (size > ESP_NOW_MAX_DATA_LENGTH) {
        D_PRINTF("EspNow: sending data to big: %i (max %i) \r\n", size, ESP_NOW_MAX_DATA_LENGTH);
        return false;
    }

    register_peer(mac_addr);

    int current_offset = 0;
    int paket_index = 0;

    uint8_t total_count = size / ESP_NOW_MAX_PACKET_DATA_LENGTH + (size % ESP_NOW_MAX_PACKET_DATA_LENGTH ? 1 : 0);
    EspNowPacketHeader packet{
            .id = _id++,
            .count = total_count,
    };

    uint8_t packet_data[ESP_NOW_PACKET_HEADER_LENGTH + ESP_NOW_MAX_PACKET_DATA_LENGTH];

    while (current_offset < size) {
        packet.index = paket_index;
        packet.size = std::min(size - current_offset, ESP_NOW_MAX_PACKET_DATA_LENGTH);

        memcpy(packet_data, &packet, sizeof(packet));
        memcpy(packet_data + sizeof(packet), data + current_offset, packet.size);

        auto packet_size = ESP_NOW_PACKET_HEADER_LENGTH + packet.size;
        auto ret = esp_now_send(mac_addr, packet_data, packet_size);

        if (ret != ESP_OK) {
            D_PRINTF("EspNow: failed to send message %i packet %i/%i, err_code: 0x%x\r\n", packet.id, packet.index + 1, packet.count, ret);

            return false;
        }

        D_PRINTF("EspNow: send message %i packet %i/%i, size %i\r\n", packet.id, packet.index + 1, packet.count, packet_size);

        ++paket_index;
        current_offset += packet.size;
    }

    return true;
}

bool EspNow::send(const uint8_t *mac_addr, const char *str) {
    return send(mac_addr, (uint8_t *) str, (int) strlen(str));
}

bool EspNow::ping(const uint8_t *mac_addr) {
    return send_packet<nullptr_t>(mac_addr, PacketType::PING, 0, nullptr);
}

void EspNow::register_peer(const uint8_t *mac_addr) {
    if (!_initialized || esp_now_is_peer_exist(mac_addr)) return;

    D_WRITE("EspNow: register new peer ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    esp_now_peer_info peer{};
    peer.channel = _peers.size();
    peer.encrypt = false;
    memcpy(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

    _peers.push_back(peer);

    auto ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK) {
        D_PRINTF("EspNow: unable to register peer: %i\r\n", ret);
        _peers.erase(_peers.end() - 1);
    }
}

void EspNow::received(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    if (data_len <= ESP_NOW_PACKET_HEADER_LENGTH) {
        D_PRINTF("EspNow: received data too small: %i\r\n", data_len);
        return;
    }

    EspNowPacketHeader header; // NOLINT(*-pro-type-member-init)
    memcpy(&header, data, sizeof(EspNowPacketHeader));

    const uint8_t *packet_data = data + ESP_NOW_PACKET_HEADER_LENGTH;
    const uint8_t packet_data_size = data_len - ESP_NOW_PACKET_HEADER_LENGTH;

    auto &self = instance();

    const auto &it = self._receiving_data.find(header.id);
    bool meta_exists = it != self._receiving_data.end();
    if (!meta_exists) {
        if (header.index == 0 && header.count > 0) {
            self._receiving_data[header.id] = {
                    .created_at = millis(),
                    .id = header.id,
                    .index = 0,
                    .count = header.count,
                    .size = 0,
                    .data = new uint8_t[ESP_NOW_MAX_PACKET_DATA_LENGTH * header.count]
            };
        } else {
            D_PRINTF("EspNow: received invalid initial packet %i for message %i\r\n", header.index, header.id);
            return;
        }
    } else {
        auto &meta = it->second;
        if (header.index != meta.index + 1 || header.count != meta.count) {
            D_PRINTF("EspNow: received unexpected packet %i for message %i\r\n", header.index, header.id);
            self._erase_receiving_data(header.id);
            return;
        }
    }

    self._process_packet(mac_addr, header, packet_data, packet_data_size);
}

void EspNow::sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        D_PRINTF("EspNow: failed to send data, err_code: 0x%x\r\n", status);
    }
    auto &self = EspNow::instance();
    if (self._on_sent_cb) self._on_sent_cb(mac_addr, status);
}

void EspNow::_process_packet(const uint8_t *mac_addr, const EspNowPacketHeader &packet, const uint8_t *data, uint8_t data_size) {
    auto &meta = _receiving_data[packet.id];

    memcpy(meta.data + meta.size, data, data_size);
    meta.index = packet.index;
    meta.size += packet.size;

    D_PRINTF("EspNow: received message %i packet %i/%i, size %i\r\n", packet.id, packet.index + 1, packet.count, packet.size);

    if (meta.index == packet.count - 1) {
        D_PRINTF("EspNow: received message id %i, size: %i\r\n", meta.id, meta.size);

        if (_on_message_cb) {
            _on_message_cb(mac_addr, meta.data, meta.size);
        } else {
            D_PRINT("EspNow: no callback registered");
        }

        _erase_receiving_data(packet.id);
    }
}

void EspNow::_erase_receiving_data(uint8_t id) {
    auto it = _receiving_data.find(id);
    if (it == _receiving_data.end()) return;

    auto &meta = it->second;
    delete[] meta.data;
    _receiving_data.erase(id);

    VERBOSE(D_PRINTF("EspNow: erase message %i\r\n", id));
}

bool EspNow::configure_channel(const uint8_t *mac_addr) {
    if (!_initialized) return false;

    if (wifi_channel_found && !_check_channel(mac_addr, wifi_channel)) {
        D_PRINT("EspNow: Saved channel not working. Reset.");
        wifi_channel_found = false;
    }

    if (wifi_channel_found) return true;

    auto channel = _find_channel(mac_addr);
    if (channel == 0xff) return false;

    wifi_channel = channel;
    wifi_channel_found = true;
    return true;
}

void EspNow::_change_channel(uint8_t channel) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    D_PRINTF("EspNow: Change WiFi Channel %i\r\n", channel);
}

bool EspNow::_check_channel(const uint8_t *mac_addr, uint8_t channel) {
    bool callback_called = false;
    bool send_success = false;

    auto prev_cb = std::move(_on_sent_cb);

    set_on_sent([&](auto, auto status) {
        callback_called = true;
        send_success = status == ESP_NOW_SEND_SUCCESS;
    });

    _change_channel(channel);

    bool ok = ping(mac_addr);
    while (ok && !callback_called) delay(10);

    set_on_sent(std::move(prev_cb));
    return send_success;
}

uint8_t EspNow::_find_channel(const uint8_t *mac_addr) {
    D_PRINT("EspNow: Finding WiFi channel...");

    auto now = millis();
    uint8_t channel = 0;

    while (millis() - now < 5000) {
        D_PRINTF("EspNow: Trying network channel %u...\r\n", channel);

        if (_check_channel(mac_addr, channel)) return channel;
        channel = (channel + 1) % 14;

        if (channel == 0) delay(500);
    }

    return 0xff;
}
