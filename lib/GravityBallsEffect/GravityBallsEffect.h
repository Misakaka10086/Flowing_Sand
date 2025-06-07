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
    // 构造函数
    GravityBallsEffect();
    // 析构函数
    ~GravityBallsEffect();

    // 初始化方法
    template<typename T_NeoPixelBus>
    bool Begin(T_NeoPixelBus& strip, uint8_t numBalls, int8_t sdaPin = 4, int8_t sclPin = 5)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8; // Assuming 8x8 matrix for simplicity
        _matrixHeight = 8;
        
        // 初始化 ADXL345
        _accel = new Adafruit_ADXL345_Unified(12345);
        Wire.begin(sdaPin, sclPin);
        if (!_accel->begin()) {
            return false; // 初始化失败
        }
        _accel->setRange(ADXL345_RANGE_4_G);

        // 初始化小球
        setNumberOfBalls(numBalls);

        _lastUpdateTime = millis();
        return true;
    }

    // 主更新函数
    void Update();

    // --- 公共配置接口 ---
    void setNumberOfBalls(uint8_t num);
    void setGravityScale(float scale);
    void setDampingFactor(float damping);
    void setSensorDeadZone(float deadzone);
    void setRestitution(float restitution);
    void setBaseBrightness(uint8_t brightness); // 0-255
    void setBrightnessCyclePeriod(float seconds);
    void setColorCyclePeriod(float seconds);
    void setBallColorSaturation(float saturation);

private:
    // 私有状态结构体
    struct Ball {
        float x, y;
        float vx, vy;
        float brightnessFactor;
        float brightnessPhaseOffset;
        float hue;
        float huePhaseOffset;
    };

    // 私有成员变量
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    Adafruit_ADXL345_Unified* _accel = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 8;
    uint8_t _matrixHeight = 8;
    
    Ball* _balls = nullptr;
    uint8_t _numBalls = 0;

    unsigned long _lastUpdateTime = 0;

    // 私有配置参数
    float _gravityScale = 25.0f;
    float _dampingFactor = 0.95f;
    float _sensorDeadZone = 0.8f;
    float _restitution = 0.75f;
    float _ballRadius = 0.5f;
    uint8_t _baseBrightness = 64;
    float _minBrightnessScale = 0.2f;
    float _maxBrightnessScale = 1.0f;
    float _brightnessCyclePeriodS = 3.0f;
    float _colorCyclePeriodS = 10.0f;
    float _ballColorSaturation = 1.0f;

    // 私有辅助函数
    void initBalls();
};

#endif // GRAVITY_BALLS_EFFECT_H