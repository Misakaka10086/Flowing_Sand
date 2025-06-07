#include <Arduino.h>
#include <NeoPixelBus.h>

// 包含所有效果库
#include <RippleEffect.h> // <--- 包含新库

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
RippleEffect rippleEffect; // 创建波纹效果实例

void setup() {
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化波纹效果
    rippleEffect.Begin(strip);
    
    // --- (可选) 使用公共接口配置效果参数 ---
    rippleEffect.setSpeed(4.0f);         // 波纹快一点
    rippleEffect.setThickness(1.5f);     // 波纹细一点
    rippleEffect.setSpawnInterval(1.5f); // 每1.5秒一个
    rippleEffect.setRandomOrigin(true);  // 起始点随机偏移
    rippleEffect.setBrightness(0.1f);
    randomSeed(millis());
}

void loop() {
    // 调用波纹效果的Update方法
    rippleEffect.Update();

    // 显示更新
    strip.Show();

    delay(20);
}