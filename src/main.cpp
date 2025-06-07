#include <Arduino.h>
#include <NeoPixelBus.h>


#include <CodeRainEffect.h>

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
CodeRainEffect codeRain; // 创建代码雨效果实例

void setup() {
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化代码雨效果
    codeRain.Begin(strip);
    
    // --- (可选) 使用公共接口配置效果参数 ---
    codeRain.setSpeedRange(8.0f, 24.0f);
    codeRain.setBaseBrightness(220); // 让绿色更亮一些
    codeRain.setSpawnProbability(0.2f); // 稍微增加生成概率

    randomSeed(millis());
}

void loop() {
    // 调用代码雨效果的Update方法
    codeRain.Update();

    // 显示更新
    strip.Show();

    delay(30);
}