#include <Arduino.h>
#include <NeoPixelBus.h>
#include <ZenLightsEffect.h> // 包含你的新库

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
ZenLightsEffect zenEffect;

void setup() {
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化禅意效果，并传入灯带对象
    zenEffect.Begin(strip);

    // --- 使用公共接口配置效果参数 ---
    // 您可以根据喜好调整这些值
    zenEffect.setMaxActiveLeds(10);                // 最多10个活动灯珠
    zenEffect.setDurationRange(4000, 8000);     // 亮灭周期在4到8秒之间
    zenEffect.setBrightnessRange(0.1f, 0.5f);   // 峰值亮度在10%到50%之间
    zenEffect.setHueRange(0.05f, 0.15f);        // 颜色范围：暖色调 (橙色/黄色)
    zenEffect.setSaturation(0.9f);              // 饱和度高
    zenEffect.setSpawnInterval(200);            // 每200ms尝试生成一个新灯珠

    randomSeed(analogRead(A0)); // 用模拟引脚获得更好的随机种子
}

void loop() {
    // 在主循环中，只需调用效果的Update方法
    zenEffect.Update();

    // 然后统一显示更新
    strip.Show();

    // 可以在这里添加其他逻辑，或者只是一个小延时
    delay(20);
}