#ifndef MQTT_CONTROLLER_H
#define MQTT_CONTROLLER_H

#include <WiFi.h>
#include <AsyncMqttClient.h>
#include "../../../include/secrets.h" // 在头文件中包含，因为实现也在这里

typedef void (*EffectChangeCallback)(int effectIndex);

class MqttController
{
public:
    MqttController();

    // ***** Begin 的实现现在直接放在头文件中 *****
    void Begin(EffectChangeCallback callback) {
        _effectChangeCallback = callback;

        _mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)this, [](TimerHandle_t xTimer) {
            static_cast<MqttController*>(pvTimerGetTimerID(xTimer))->connectToMqtt();
        });
        _wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)this, [](TimerHandle_t xTimer) {
            static_cast<MqttController*>(pvTimerGetTimerID(xTimer))->connectToWifi();
        });

        _mqttClient.onConnect([this](bool sessionPresent) { this->onMqttConnect(sessionPresent); });
        _mqttClient.onDisconnect([this](AsyncMqttClientDisconnectReason reason) { this->onMqttDisconnect(reason); });
        _mqttClient.onSubscribe([this](uint16_t packetId, uint8_t qos) { this->onMqttSubscribe(packetId, qos); });
        _mqttClient.onMessage([this](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
            this->onMqttMessage(topic, payload, properties, len, index, total);
        });

        _mqttClient.setServer(MQTT_HOST, MQTT_PORT);
        if (strlen(MQTT_USER) > 0) {
            _mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);
        }
        
        // 使用Lambda表达式注册WiFi事件
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            Serial.printf("[WiFi-event] event: %d\n", event);
            switch(event) {
                case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                    Serial.println("WiFi connected");
                    Serial.print("IP address: ");
                    Serial.println(WiFi.localIP());
                    connectToMqtt(); 
                    break;
                case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                    Serial.println("WiFi lost connection");
                    xTimerStop(_mqttReconnectTimer, 0); 
                    xTimerStart(_wifiReconnectTimer, 0);
                    break;
                default:
                    break;
            }
        });

        connectToWifi();
    }


private:
    AsyncMqttClient _mqttClient;
    TimerHandle_t _mqttReconnectTimer;
    TimerHandle_t _wifiReconnectTimer;

    EffectChangeCallback _effectChangeCallback;

    // MQTT事件处理函数
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMqttSubscribe(uint16_t packetId, uint8_t qos);
    void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

    // WiFi连接函数
    void connectToWifi();
    void connectToMqtt();
};

#endif // MQTT_CONTROLLER_H