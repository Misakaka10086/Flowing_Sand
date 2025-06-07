#ifndef ZEN_LIGHTS_EFFECT_H
#define ZEN_LIGHTS_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class ZenLightsEffect
{
public:
    // 构造函数
    ZenLightsEffect();
    // 析构函数，用于释放动态分配的内存
    ~ZenLightsEffect();

    // 初始化方法，传入NeoPixelBus对象
    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();

        // 如果之前已分配内存，先释放
        if (_ledStates != nullptr) {
            delete[] _ledStates;
        }
        // 为LED状态动态分配内存
        _ledStates = new LedEffectState[_numLeds];

        // 初始化所有LED状态
        for (uint16_t i = 0; i < _numLeds; i++) {
            _ledStates[i].isActive = false;
        }
        _lastAttemptTimeMs = millis();
    }

    // 主更新函数，在Arduino的loop()中调用
    void Update();

    // --- 公共配置接口 ---
    void setMaxActiveLeds(int count);
    void setDurationRange(unsigned long minMs, unsigned long maxMs);
    void setBrightnessRange(float min, float max); // 0.0 - 1.0
    void setHueRange(float min, float max);      // 0.0 - 1.0
    void setSaturation(float saturation);        // 0.0 - 1.0
    void setSpawnInterval(unsigned long intervalMs);

private:
    // 私有状态结构体
    struct LedEffectState {
        bool isActive;
        unsigned long startTimeMs;
        unsigned long durationMs;
        float peakBrightnessFactor; 
        float hue;                  
    };

    // 私有成员变量
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    LedEffectState* _ledStates = nullptr;
    unsigned long _lastAttemptTimeMs = 0;

    // 私有配置参数 (带默认值)
    int _maxActiveLeds = 8;
    unsigned long _minDurationMs = 10000;
    unsigned long _maxDurationMs = 20000;
    float _minPeakBrightness = 0.05f;
    float _maxPeakBrightness = 0.1f;
    float _hueMin = 0.29f;
    float _hueMax = 0.41f;
    float _saturation = 0.9f;
    unsigned long _spawnIntervalMs = 300;

    // 私有辅助函数
    int countActiveLeds();
    void tryActivateNewLed();
};

#endif // ZEN_LIGHTS_EFFECT_H