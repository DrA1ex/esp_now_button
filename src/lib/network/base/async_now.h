#pragma once

#include <esp_now.h>
#include <queue>
#include <unordered_map>
#include <WiFi.h>

#include "../../misc/promise.h"
#include "../../debug.h"

struct EspNowPacket {
    uint8_t mac_addr[6];
    std::vector<uint8_t> data;
};

typedef std::function<void(EspNowPacket packet)> AsyncEspNowOnPacketCb;

class AsyncEspNow {
    static AsyncEspNow _instance;

    bool _initialized = false;

    std::vector<esp_now_peer_info> _peers;
    std::unordered_map<uint64_t, std::queue<std::shared_ptr<Promise<void>>>> _send_order;

    AsyncEspNowOnPacketCb _on_packet_cb {};

    AsyncEspNow() = default;

public:
    static AsyncEspNow &instance() { return _instance; }

    AsyncEspNow(const AsyncEspNow &) = delete;
    AsyncEspNow &operator=(AsyncEspNow const &) = delete;

    bool begin();

    Future<void> send(const uint8_t *mac_addr, const uint8_t *data, uint8_t size);

    bool change_channel(uint8_t channel);

    bool is_peer_exists(const uint8_t *mac_addr) const;
    bool register_peer(const uint8_t *mac_addr, uint8_t channel = 0);
    bool unregister_peer(const uint8_t *mac_addr);

    void set_on_packet_cb(AsyncEspNowOnPacketCb cb) { _on_packet_cb = std::move(cb); }

private:
    static void _on_sent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void _on_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len);
};
