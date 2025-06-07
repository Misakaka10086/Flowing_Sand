#include <Arduino.h>
#include <NeoPixelBus.h>
#include <CodeRainEffect.h> 

// WS2812 灯珠引脚定义
const int LED_PIN = 11;
const int NUM_LEDS = 64;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
CodeRainEffect effect;

void setup() {
    Serial.begin(115200);

    strip.Begin();
    strip.Show();

    effect.Begin(strip);
    
    // 加载一个预设
    effect.setParameters(CodeRainEffect::ClassicMatrixPreset);
    
    // 或者加载另一个预设
    // effect.setParameters(CodeRainTainEffect::FastGlitchPreset);

    randomSeed(millis());
}

void loop() {
    effect.Update();
    strip.Show();
    delay(30);
}