#include <Arduino.h>
#include <EffectController.h>
#include <MqttController.h>
#include <NeoPixelBus.h>

const int LED_PIN = 11;
const int NUM_LEDS = 64 * 4;

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
EffectController effectController;
MqttController mqttController;

// 回调函数：将MQTT收到的指令转发给效果控制器
void onCommandReceived(const char *command) {
  Serial.printf("Command received by main: %s\n", command);
  effectController.processCommand(command);
}

void setup() {
  Serial.begin(115200);

  strip.Begin();
  strip.Show();

  // 初始化效果控制器，它会处理所有效果的初始化
  effectController.Begin(strip);

  // 初始化MQTT控制器，并把它和效果控制器连接起来
  mqttController.Begin(onCommandReceived);

  randomSeed(millis());
}

void loop() {
  // 主循环只做两件事：更新效果和显示
  effectController.Update();
  strip.Show();
}