#include <Arduino.h>
#include <NeoPixelBus.h>

// 包含所有效果库和网络控制器
#include <ZenLightsEffect.h> 
#include <GravityBallsEffect.h>
#include <CodeRainEffect.h>
#include <RippleEffect.h>
#include <MqttController.h>

// --- 定义 ---
const int LED_PIN = 11;
const int NUM_LEDS = 64;

// --- 全局实例 ---
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
MqttController mqttController;

// 效果实例
ZenLightsEffect zenEffect;
GravityBallsEffect gravityEffect;
CodeRainEffect codeRainEffect;
RippleEffect rippleEffect;

// 全局状态变量，用于跟踪当前效果
volatile int g_currentEffectIndex = 0; // 使用 volatile 因为它可能在中断上下文被修改(虽然这里不是)
                                       // 但在多线程或事件驱动模型中是好习惯

// --- 回调函数 ---
// 当MQTT控制器收到指令时，会调用这个函数
void onEffectChange(int newEffectIndex) {
    Serial.printf("Callback received: Change to effect %d\n", newEffectIndex);
    if (newEffectIndex >= 0 && newEffectIndex < 4) { // 假设我们有4个效果
        g_currentEffectIndex = newEffectIndex;
        // 可以在这里重置效果参数，如果需要的话
        // 例如: rippleEffect.setParameters(RippleEffect::WaterDropPreset);
    }
}


void setup() {
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化所有效果
    zenEffect.Begin(strip);
    if (!gravityEffect.Begin(strip)) {
        Serial.println("Gravity Effect FAILED to init (ADXL345 missing?)");
    }
    codeRainEffect.Begin(strip);
    rippleEffect.Begin(strip);

    // 初始化MQTT控制器，并传入回调函数
    mqttController.Begin(onEffectChange);

    randomSeed(millis());
}

void loop() {
    // 根据当前效果索引，调用对应效果的Update方法
    switch (g_currentEffectIndex) {
        case 0:
            gravityEffect.Update();
            break;
        case 1:
            zenEffect.Update();
            break;
        case 2:
            codeRainEffect.Update();
            break;
        case 3:
            rippleEffect.Update();
            break;
        default:
            // 默认显示一个效果，或什么都不做
            gravityEffect.Update();
            break;
    }

    // 统一显示更新
    strip.Show();

    // 延时
    delay(20);
}