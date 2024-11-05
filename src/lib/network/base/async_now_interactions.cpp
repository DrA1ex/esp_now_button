#include "async_now_interactions.h"

AsyncEspNowInteraction AsyncEspNowInteraction::_instance = {};
const uint8_t AsyncEspNowInteraction::BROADCAST_MAC[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

bool AsyncEspNowInteraction::begin() {
    if (_initialized) return false;

    auto ok = _async_now.begin();
    if (!ok) return false;

    _async_now.set_on_packet_cb([this](auto packet) { _on_packet_received(std::move(packet)); });

    _initialized = true;
    return true;
}

Future<SendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const char *str) {
    return send(mac_addr, (uint8_t *) str, strlen(str));
}

Future<SendResponse> AsyncEspNowInteraction::send(const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (!_initialized) return Future<SendResponse>::errored();

    return _send_impl(_id++, mac_addr, data, size);
}

Future<EspNowPacket> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const char *str) {
    return request(mac_addr, (uint8_t *) str, strlen(str));
}

Future<EspNowPacket> AsyncEspNowInteraction::request(const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (!_initialized) return Future<EspNowPacket>::errored();

    return _request_impl(_id++, mac_addr, data, size);
}

Future<uint8_t> AsyncEspNowInteraction::discover_peer_channel(const uint8_t *mac_addr) {
    D_WRITE("EspNowInteraction: Discovering peer channel: ");
    D_PRINT_HEX(mac_addr, ESP_NOW_ETH_ALEN);

    if (!_async_now.is_peer_exists(mac_addr)) _async_now.register_peer(mac_addr);

    auto channel = std::make_shared<uint8_t>(0);
    return PromiseBase::sequential<uint8_t>(
        _configure_peer_channel(mac_addr, (*channel)++),
        [channel](auto future) { return !future.success() && *channel < 14; },
        [this, mac_addr, channel=std::move(channel)](auto) {
            return _configure_peer_channel(mac_addr, (*channel)++);
        }
    );
}

Future<SendResponse> AsyncEspNowInteraction::_send_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (data == nullptr || size == 0) {
        D_PRINT("EspNowInteraction: data missing");
        return Future<SendResponse>::errored();
    }
    if (size > ESP_NOW_INTERACTION_MAX_DATA_LENGTH) {
        D_PRINTF("EspNowInteraction: sending data to big: %i (max %i) \r\n", size, ESP_NOW_INTERACTION_MAX_DATA_LENGTH);
        return Future<SendResponse>::errored();
    }

    const uint8_t total_packets_count = size / ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH
            + (size % ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH ? 1 : 0);

    const uint8_t *data_ptr = data;
    uint8_t index = 0;

    std::vector<Future<void>> futures;
    uint16_t remaining = size;
    while (remaining > 0) {
        const auto packet_data_size = (uint8_t) std::min<uint16_t>(remaining, ESP_NOW_INTERACTION_MAX_PACKET_DATA_LENGTH);
        uint8_t packet[ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH + packet_data_size];

        auto *header = (EspNowInteractionPacketHeader *) packet;
        *header = {
            .id = id,
            .index = index++,
            .count = total_packets_count,
            .size = packet_data_size,
        };

        memcpy(packet + ESP_NOW_INTERACTION_PACKET_HEADER_LENGTH, data_ptr, packet_data_size);

        auto future = _async_now.send(mac_addr, packet, sizeof(packet));
        futures.push_back(std::move(future));

        D_PRINTF("EspNowInteraction: sending message %i packet %i/%i, size %i\r\n",
            header->id, header->index + 1, header->count, header->size);

        data_ptr += packet_data_size;
        remaining -= packet_data_size;
    }

    return PromiseBase::all(futures).then<SendResponse>([id](const FutureBase &) {
        return Future<SendResponse>::successful({.id = id});
    });
}

Future<EspNowPacket> AsyncEspNowInteraction::_request_impl(uint8_t id, const uint8_t *mac_addr, const uint8_t *data, uint16_t size) {
    if (auto it = _requests.find(id); it != _requests.end()) {
        D_PRINTF("EspNowInteraction: request %i already exist. Cancelling...\r\n", id);

        // Acquire shared pointer to avoid destruction right after ::erase()
        auto existing_promise = it->second;
        _requests.erase(it);

        existing_promise->set_error();
    }

    auto promise = Promise<EspNowPacket>::create();
    _requests[id] = promise;

    auto send_future = _send_impl(id, mac_addr, data, size);
    return send_future.then<EspNowPacket>([this, promise = std::move(promise)](const auto &future) {
        auto response = future.result();
        VERBOSE(D_PRINTF("EspNowInteraction: request %i sent. Waiting for response...\r\n", response.id));
        return Future {promise};
    }).on_error([this, id](auto future) {
        _requests.erase(id);
        return future;
    });
}

Future<uint8_t> AsyncEspNowInteraction::_configure_peer_channel(const uint8_t *mac_addr, uint8_t channel) {
    if (channel > 14 || !_async_now.change_channel(channel)) return Future<uint8_t>::errored();

    D_PRINTF("EspNowInteraction: Trying channel %i...\r\n", channel + 1);

    uint8_t data[1] {};
    return send(mac_addr, data, sizeof(data))
           .then<uint8_t>([=](auto) {
               D_PRINTF("EspNowInteraction: Channel %i is valid!\r\n", channel + 1);
               return channel;
           }).on_error([=](auto future) {
               D_PRINTF("EspNowInteraction: Channel %i isn't valid!\r\n", channel + 1);
               return future;
           });
}

void AsyncEspNowInteraction::_on_packet_received(EspNowPacket packet) {
    if (packet.data.size() < sizeof(EspNowInteractionPacketHeader)) {
        D_PRINT("EspNowInteraction: received message is too small");
        return;
    }

    auto *header = (EspNowInteractionPacketHeader *) packet.data.data();
    if (header->count > 1) {
        D_PRINTF("EspNowInteraction: received partial message id %i, which not supported\r\n", header->id);
        return;
    }

    //TODO: add support for partial requests

    auto it = _requests.find(header->id);
    if (it == _requests.end()) {
        D_PRINTF("EspNowInteraction: received unexpected message id %i\r\n", header->id);
        return;
    }

    D_PRINTF("EspNowInteraction: received message id %i\r\n", header->id);

    // Acquire shared pointer to avoid destruction right after ::erase()
    auto promise = std::move(it->second);
    _requests.erase(it);

    promise->set_success(packet);
}
