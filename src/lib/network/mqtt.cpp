#include "mqtt.h"

void MqttServer::set_prefix(String str) {
    _topic_prefix = std::move(str);
}

void MqttServer::register_command(String topic, MqttCommand command) {
    _commands[std::move(topic)] = std::move(command);
}

void MqttServer::send(const String &topic, const String &str) {
    _publish_impl((_topic_prefix + topic).c_str(), 1, str.c_str(), str.length());
}

void MqttServer::begin(const char *host, uint16_t port, const char *user, const char *password) {
    if (_state != MqttServerState::UNINITIALIZED) return;

    using namespace std::placeholders;

    _mqtt_client.onConnect(std::bind(&MqttServer::_on_connect, this, _1));
    _mqtt_client.onDisconnect(std::bind(&MqttServer::_on_disconnect, this, _1));
    _mqtt_client.onMessage(std::bind(&MqttServer::_on_message, this, _1, _2, _3, _4, _5, _6));

    _mqtt_client.setServer(host, port);
    _mqtt_client.setCredentials(user, password);

    _connect();
}

void MqttServer::handle_connection() {
    if (_state == MqttServerState::UNINITIALIZED) return;

    unsigned long current_time = millis();
    if (_state == MqttServerState::DISCONNECTED && (current_time - _last_connection_attempt_time) > MQTT_RECONNECT_TIMEOUT) {
        D_PRINT("MQTT Reconnecting...");
        _connect();
    }

    if (_state == MqttServerState::CONNECTING && !_mqtt_client.connected() &&
        (current_time - _state_change_time) > MQTT_CONNECTION_TIMEOUT) {
        D_PRINT("MQTT Connection timeout");
        _change_state(MqttServerState::DISCONNECTED);
        _mqtt_client.disconnect(true);
    }
}


void MqttServer::_connect() {
    _change_state(MqttServerState::CONNECTING);

    _last_connection_attempt_time = millis();
    _mqtt_client.connect();
}

void MqttServer::_change_state(MqttServerState state) {
    _state = state;
    _state_change_time = millis();
}

void MqttServer::_on_connect(bool) {
    D_PRINT("MQTT Connected");

    for (const auto &[topic, _]: _commands) _subscribe(topic.c_str());

    _last_connection_attempt_time = millis();
    _change_state(MqttServerState::CONNECTED);
}

void MqttServer::_on_disconnect(AsyncMqttClientDisconnectReason reason) {
    D_PRINTF("MQTT Disconnected. Reason %u\r\n", (uint8_t) reason);

    _change_state(MqttServerState::DISCONNECTED);
}

void MqttServer::_on_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                             size_t len, size_t index, size_t total) {
    D_PRINTF("MQTT Received: %s: \"%.*s\"\r\n", topic, len, payload);

    bool prefix_match = true;
    auto *cut_topic = topic;

    for (int i = 0; i < _topic_prefix.length(); ++i) {
        if (*cut_topic == '\0' || *cut_topic != _topic_prefix[i]) {
            prefix_match = false;
            break;
        }

        ++cut_topic;
    }

    String topic_str(prefix_match ? cut_topic : topic);
    String payload_str(payload, len);

    _process_message(topic_str, payload_str);
}

void MqttServer::_subscribe(const String &topic) {
    _subscribe_impl((_topic_prefix + topic).c_str(), 1);
}

void MqttServer::_subscribe_impl(const char *topic, uint8_t qos) {
    _mqtt_client.subscribe(topic, qos);
    D_PRINTF("MQTT Subscribe: \"%s\"\r\n", topic);
}

void MqttServer::_publish_impl(const char *topic, uint8_t qos, const char *payload, size_t length) {
    if (_state != MqttServerState::CONNECTED) {
        D_PRINTF("MQTT Not connected. Skip message to %s\r\n", topic);
        return;
    }

    _mqtt_client.publish(topic, qos, true, payload, length);

    D_PRINTF("MQTT Publish: %s: \"%.*s\"\r\n", topic, length, payload);
}

void MqttServer::_process_message(const String &topic, const String &payload) {
    if (auto cmd_it = _commands.find(topic); cmd_it != _commands.end()) {
        cmd_it->second(topic, payload);
    } else {
        D_PRINTF("MQTT: Message in unsupported topic: %s\r\n", topic.c_str());
    }
}