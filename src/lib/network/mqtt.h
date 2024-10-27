#pragma once

#include <AsyncMqttClient.h>
#include <map>
#include <type_traits>

#include "../debug.h"

#ifndef MQTT_CONNECTION_TIMEOUT
#define MQTT_CONNECTION_TIMEOUT                 (15000u)
#endif

#ifndef MQTT_RECONNECT_TIMEOUT
#define MQTT_RECONNECT_TIMEOUT                  (5000u)
#endif

enum class MqttServerState : uint8_t {
    UNINITIALIZED,
    CONNECTING,
    CONNECTED,
    DISCONNECTED
};

typedef std::function<void(const String &topic, const String &payload)> MqttCommand;

class MqttServer {
    String _topic_prefix;

    std::map<String, MqttCommand> _commands;

public:
    MqttServer() = default;

    void set_prefix(String str);

    void begin(const char *host, uint16_t port, const char *user, const char *password);
    void handle_connection();

    void register_command(String topic, MqttCommand command);

    template<typename T, typename = decltype(std::is_constructible_v<T, String>)>
    void send(const String &topic, const T &value);

    void send(const String &topic, const String &str);

private:
    AsyncMqttClient _mqtt_client;

    MqttServerState _state = MqttServerState::UNINITIALIZED;
    unsigned long _state_change_time = 0;
    unsigned long _last_connection_attempt_time = 0;

    void _on_connect(bool session_present);
    void _on_disconnect(AsyncMqttClientDisconnectReason reason);
    void _on_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

    void _subscribe(const String &topic);
    void _subscribe_impl(const char *topic, uint8_t qos);

    void _publish_impl(const char *topic, uint8_t qos, const char *payload, size_t length);

    void _process_message(const String &topic, const String &payload);

    void _change_state(MqttServerState state);
    void _connect();
};

template<typename T, typename>
void MqttServer::send(const String &topic, const T &value) {
    send(topic, String(value));
}
