#include "async_now.h"

uint64_t mac_to_key(const uint8_t *mac_addr) {
    uint64_t mac_addr_key = 0;
    memcpy(&mac_addr_key, mac_addr, ESP_NOW_ETH_ALEN);

    return mac_addr_key;
}

AsyncEspNow AsyncEspNow::_instance {};

bool AsyncEspNow::begin() {
    if (_initialized) return false;

    if (WiFiClass::getMode() == WIFI_MODE_NULL) WiFiClass::mode(WIFI_MODE_STA);

    auto ret = esp_now_init();
    if (ret != ESP_OK) {
        D_PRINTF("AsyncEspNow: Unable to initialize ESP NOW: %i\r'n", ret);
        return false;
    }

    bool cb_init_success = esp_now_register_send_cb(AsyncEspNow::_on_sent) == ESP_OK;
    cb_init_success = cb_init_success && esp_now_register_recv_cb(AsyncEspNow::_on_receive) == ESP_OK;

    if (!cb_init_success) {
        esp_now_deinit();

        D_PRINT("AsyncEspNow: Unable to initialize ESP NOW: failed to attach callbacks");
        return false;
    }

    _initialized = true;
    return true;
}

Future<void> AsyncEspNow::send(const uint8_t *mac_addr, const uint8_t *data, uint8_t size) {
    bool success = _initialized;
    success = success && register_peer(mac_addr);
    success = success && esp_now_send(mac_addr, data, size) == ESP_OK;

    if (!success) {
        D_PRINT("AsyncEspNow: failed to send packet");
        return Future<void>::errored();
    }

    D_PRINT("AsyncEspNow: sending packet");
    D_WRITE("\t- Destination: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
    D_PRINTF("\t- Size: %i\n", size);

    auto promise = std::make_shared<Promise<void>>();
    _send_order[mac_to_key(mac_addr)].push(promise);

    return Future {promise};
}

bool AsyncEspNow::register_peer(const uint8_t *mac_addr) {
    if (!_initialized) return false;
    if (esp_now_is_peer_exist(mac_addr)) return true;

    //TODO: Implement unregister_peer and reuse channels
    esp_now_peer_info peer {};
    peer.channel = 0; //TODO: Use custom channels for every peer (?)
    peer.encrypt = false;
    memcpy(peer.peer_addr, mac_addr, ESP_NOW_ETH_ALEN);

    auto ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK) {
        D_PRINTF("AsyncEspNow: unable to register peer: %i\r\n", ret);
        return false;
    }

    D_WRITE("AsyncEspNow: register new peer ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    uint64_t mac_addr_key = mac_to_key(mac_addr);
    _send_order[mac_addr_key] = {};
    _peers.push_back(peer);

    return true;
}

void AsyncEspNow::_on_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    auto &self = AsyncEspNow::instance();

    uint64_t mac_addr_key = mac_to_key(mac_addr);
    auto it = self._send_order.find(mac_addr_key);
    if (it == self._send_order.end() || it->second.empty()) {
        D_WRITE("AsyncEspNow: Unexpected sent event. Destination: ");
        D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
        return;
    }

    VERBOSE(D_PRINT("AsyncEspNow: received sent event"));

    auto &queue = it->second;
    const auto promise = queue.front();
    queue.pop();

    if (status == ESP_NOW_SEND_SUCCESS) {
        VERBOSE(D_WRITE("AsyncEspNow: send confirmed "));
        VERBOSE(D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN));

        promise->set_success();
    } else {
        D_WRITE("AsyncEspNow: error while sending data. Destination: ");
        D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

        promise->set_error();
    }
}

void AsyncEspNow::_on_receive(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    EspNowPacket packet {};

    memcpy(packet.mac_addr, mac_addr, sizeof(packet.mac_addr));
    packet.data.assign(data, data + data_len);

    D_PRINT("AsyncEspNow: received packet");
    D_WRITE("\t- Sender: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);
    D_PRINTF("\t- Size: %i\n", data_len);

    auto &self = AsyncEspNow::instance();
    if (self._on_packet_cb) {
        self._on_packet_cb(std::move(packet));
    }
}
