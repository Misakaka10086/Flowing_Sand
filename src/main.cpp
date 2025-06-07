#include <Arduino.h>
#include <NeoPixelBus.h>
// 包含两个效果库
#include <ZenLightsEffect.h>
#include <GravityBallsEffect.h>
#include <CodeRainEffect.h>
#include <RippleEffect.h> // <--- 包含新库

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 创建 NeoPixelBus 和 效果类的实例
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);
ZenLightsEffect zenEffect;        // 创建禅意效果实例
GravityBallsEffect gravityEffect; // 创建重力球效果实例
CodeRainEffect codeRain;          // 创建代码雨效果实例
RippleEffect rippleEffect;        // 创建波纹效果实例

void setup()
{
    Serial.begin(115200);

    // 初始化灯带
    strip.Begin();
    strip.Show();

    // 初始化禅意效果，并传入灯带对象
    zenEffect.Begin(strip);

    // --- 使用公共接口配置效果参数 ---
    // 您可以根据喜好调整这些值
    zenEffect.setMaxActiveLeds(10);           // 最多10个活动灯珠
    zenEffect.setDurationRange(4000, 8000);   // 亮灭周期在4到8秒之间
    zenEffect.setBrightnessRange(0.1f, 0.5f); // 峰值亮度在10%到50%之间
    zenEffect.setHueRange(0.05f, 0.15f);      // 颜色范围：暖色调 (橙色/黄色)
    zenEffect.setSaturation(0.9f);            // 饱和度高
    zenEffect.setSpawnInterval(200);          // 每200ms尝试生成一个新灯珠

    // 初始化重力球效果
    Serial.println("Initializing Gravity Balls Effect...");
    if (gravityEffect.Begin(strip, 20))
    { // 传入灯带对象和20个小球
        Serial.println("Gravity Balls Effect Initialized Successfully.");
    }
    else
    {
        Serial.println("Failed to initialize Gravity Balls Effect (ADXL345 not found).");
        while (1)
            ; // 卡住
    }

    // --- (可选) 使用公共接口配置效果参数 ---
    gravityEffect.setGravityScale(30.0f);
    gravityEffect.setDampingFactor(0.92f);
    gravityEffect.setRestitution(0.6f);
    // 其他参数使用默认值...

    // 初始化代码雨效果
    codeRain.Begin(strip);

    // --- (可选) 使用公共接口配置效果参数 ---
    codeRain.setSpeedRange(8.0f, 24.0f);
    codeRain.setBaseBrightness(220);    // 让绿色更亮一些
    codeRain.setSpawnProbability(0.2f); // 稍微增加生成概率

    // 初始化波纹效果
    rippleEffect.Begin(strip);

    // --- (可选) 使用公共接口配置效果参数 ---
    rippleEffect.setSpeed(4.0f);         // 波纹快一点
    rippleEffect.setThickness(1.5f);     // 波纹细一点
    rippleEffect.setSpawnInterval(1.5f); // 每1.5秒一个
    rippleEffect.setRandomOrigin(true);  // 起始点随机偏移
    rippleEffect.setBrightness(0.1f);
    randomSeed(analogRead(A0));
}

void loop()
{

    // zenEffect.Update();
    // gravityEffect.Update();
    // codeRain.Update();
    rippleEffect.Update();
    // 显示更新
    strip.Show();

    delay(10); // 可以用一个稍小的延时
}