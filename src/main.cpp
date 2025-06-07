#include <Arduino.h>
#include <NeoPixelBus.h>
// 包含两个效果库
// #include <ZenLightsEffect.h> 
#include <GravityBallsEffect.h>

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
GravityBallsEffect gravityEffect; // 创建重力球效果实例

// ---- (可选) 如果想同时运行或切换效果 ----
// ZenLightsEffect zenEffect;
// int currentEffect = 0; // 0 for gravity, 1 for zen

void setup() {
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化重力球效果
    Serial.println("Initializing Gravity Balls Effect...");
    if (gravityEffect.Begin(strip, 20)) { // 传入灯带对象和20个小球
        Serial.println("Gravity Balls Effect Initialized Successfully.");
    } else {
        Serial.println("Failed to initialize Gravity Balls Effect (ADXL345 not found).");
        while(1); // 卡住
    }

    // --- (可选) 使用公共接口配置效果参数 ---
    gravityEffect.setGravityScale(30.0f);
    gravityEffect.setDampingFactor(0.92f);
    gravityEffect.setRestitution(0.6f);
    // 其他参数使用默认值...
    
    randomSeed(analogRead(A0));
}

void loop() {
    // 调用重力球效果的Update方法
    gravityEffect.Update();

    // 显示更新
    strip.Show();

    delay(10); // 可以用一个稍小的延时
}