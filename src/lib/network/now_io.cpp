#include "now_io.h"

#include <lib/misc/system_timer.h>

NowIo NowIo::_instance {};

bool NowIo::begin() { return _interaction.begin(); }

Future<void> NowIo::send_packet(const uint8_t *mac_addr, uint8_t type) {
    return send_packet(mac_addr, type, 0, nullptr, 0);
}

Future<EspNowPacket> NowIo::request_packet(const uint8_t *mac_addr, uint8_t type) {
    return request_packet(mac_addr, type, 0, nullptr, 0);
}

Future<void> NowIo::send_packet(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(PacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.send(mac_addr, packet, sizeof(packet));
}

Future<EspNowPacket> NowIo::request_packet(const uint8_t *mac_addr, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    uint8_t packet[sizeof(PacketHeader) + size];
    _fill_packet_data(packet, type, count, data, size);

    return _interaction.request(mac_addr, packet, sizeof(packet));
}

Future<uint8_t> NowIo::discover_hub(uint8_t *out_mac_addr) {
    D_PRINT("NowIo: Discovering hub...");

    auto channel = std::make_shared<uint8_t>(0);
    auto discovery_future = PromiseBase::sequential<uint8_t>(
        _discover_hub_channel((*channel)++, out_mac_addr),
        [channel](auto future) { return !future.success() && *channel < 14; },
        [this, out_mac_addr, channel](auto) {
            return _discover_hub_channel((*channel)++, out_mac_addr);
        }
    );

    // Verify hub addr and channel
    return discovery_future
           .then<void>([this, out_mac_addr](auto) {
               D_PRINT("NowIo: Verifying hub...");
               return send_packet(out_mac_addr, (uint8_t) SpecialPacketTypes::PING);
           }).then<uint8_t>([discovery_future](auto) {
               D_PRINT("NowIo: Hub verified...");
               return discovery_future;
           });
}

void NowIo::_fill_packet_data(uint8_t *out_packet, uint8_t type, uint8_t count, const uint8_t *data, uint16_t size) {
    auto *header = (PacketHeader *) out_packet;
    *header = {.type = type, .count = count};
    memcpy(out_packet + sizeof(PacketHeader), data, size);
}

Future<uint8_t> NowIo::_discover_hub_channel(uint8_t channel, uint8_t *out_mac_addr) {
    if (channel > 14 || !AsyncEspNow::instance().change_channel(channel)) return Future<uint8_t>::errored();

    D_PRINTF("NowIo: Trying to discover hub at channel %i...\r\n", channel + 1);

    auto request_future = request_packet(AsyncEspNowInteraction::BROADCAST_MAC, (uint8_t) SpecialPacketTypes::DISCOVERY);
    auto delay_future = SystemTimer::delay(100);

    return PromiseBase::any({request_future, delay_future})
           .then<uint8_t>([=](auto) {
               if (!request_future.finished() || !request_future.success()) return Future<uint8_t>::errored();

               D_PRINTF("NowIo: Hub respond at channel %i!\r\n", channel + 1);
               memcpy(out_mac_addr, request_future.result().mac_addr, sizeof(AsyncEspNowInteraction::BROADCAST_MAC));
               return Future<uint8_t>::successful(channel);
           }).on_error([=](auto future) {
               D_PRINTF("NowIo: Hub doesn't respond on channel %i\r\n", channel + 1);
               return future;
           });
}
