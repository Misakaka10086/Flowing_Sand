#include "MqttController.h"
// "secrets.h" 不再需要在这里包含，因为它已经在头文件中了

MqttController::MqttController() {
    _commandCallback = nullptr;
}

// ***** Begin 的实现已移至 MqttController.h *****


void MqttController::connectToWifi() {
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void MqttController::connectToMqtt() {
    Serial.println("Connecting to MQTT...");
    _mqttClient.connect();
}

void MqttController::onMqttConnect(bool sessionPresent) {
    Serial.println("Connected to MQTT.");
    uint16_t packetIdSub = _mqttClient.subscribe(MQTT_TOPIC_COMMAND, 2);
    Serial.printf("Subscribing to %s...\n", MQTT_TOPIC_COMMAND);
    _mqttClient.publish(MQTT_TOPIC_STATUS, 0, true, "{\"status\":\"online\"}");
}

void MqttController::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
    Serial.print("Disconnected from MQTT. Reason: ");
    switch (reason) {
        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED: Serial.println("TCP Disconnected"); break;
        case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION: Serial.println("Unacceptable Protocol Version"); break;
        case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED: Serial.println("Identifier Rejected"); break;
        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: Serial.println("Server Unavailable"); break;
        case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS: Serial.println("Malformed Credentials"); break;
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED: Serial.println("Not Authorized"); break;
        case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE: Serial.println("Not Enough Space"); break;
        case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT: Serial.println("TLS Bad Fingerprint"); break;
        default: Serial.println("Unknown"); break;
    }

    if (WiFi.isConnected()) {
        xTimerStart(_mqttReconnectTimer, 0);
    }
}

void MqttController::onMqttSubscribe(uint16_t packetId, uint8_t qos) {
    Serial.println("Subscribe acknowledged.");
}

void MqttController::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    char message[len + 1];
    memcpy(message, payload, len);
    message[len] = '\0';
    Serial.printf("Message received on topic %s: %s\n", topic, message);

    if (_commandCallback != nullptr) {
        _commandCallback(message); // 直接传递整个 payload
    }
}