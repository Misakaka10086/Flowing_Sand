#ifndef GRAVITY_BALLS_EFFECT_H
#define GRAVITY_BALLS_EFFECT_H

#include <NeoPixelBus.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

class GravityBallsEffect
{
public:
    struct Parameters {
        uint8_t numBalls;
        float gravityScale;
        float dampingFactor;
        float sensorDeadZone;
        float restitution;
        uint8_t baseBrightness;
        float brightnessCyclePeriodS;
        float minBrightnessScale;
        float maxBrightnessScale;
        float colorCyclePeriodS;
        float ballColorSaturation;
    };

    static const Parameters BouncyPreset;
    static const Parameters PlasmaPreset;

    GravityBallsEffect();
    ~GravityBallsEffect();

    // Begin函数现在只接收strip对象
    template<typename T_NeoPixelBus>
    bool Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8;
        _matrixHeight = 8;
        
        // ADXL345的初始化现在可以硬编码引脚，或者也放入参数结构体
        // 为了简单，我们先硬编码
        if (!_accel) {
             _accel = new Adafruit_ADXL345_Unified(12345);
        }
        Wire.begin(4, 5); // Default SDA=4, SCL=5
        if (!_accel->begin()) {
            return false;
        }
        _accel->setRange(ADXL345_RANGE_4_G);

        initBalls();

        _lastUpdateTime = millis();
        return true;
    }

    void Update();

    // --- 重载的 setParameters 方法 ---
    void setParameters(const Parameters& params); // 接受结构体
    void setParameters(const char* jsonParams);   // 接受JSON字符串

private:
    struct Ball {
        float x, y;
        float vx, vy;
        float brightnessFactor;
        float brightnessPhaseOffset;
        float hue;
        float huePhaseOffset;
    };

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    Adafruit_ADXL345_Unified* _accel = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 8;
    uint8_t _matrixHeight = 8;
    
    Ball* _balls = nullptr;
    
    unsigned long _lastUpdateTime = 0;
    
    Parameters _params; // 使用参数结构体存储所有配置

    void initBalls();
};

#endif // GRAVITY_BALLS_EFFECT_H