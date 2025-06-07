#ifndef RIPPLE_EFFECT_H
#define RIPPLE_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class RippleEffect
{
public:
    RippleEffect();
    ~RippleEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8; // Assuming 8x8
        _matrixHeight = 8;

        if (_ripples != nullptr) {
            delete[] _ripples;
        }
        _ripples = new Ripple[_maxRipples];

        for (uint8_t i = 0; i < _maxRipples; ++i) {
            _ripples[i].isActive = false;
        }
        _lastAutoRippleTimeMs = millis();
    }

    void Update();

    // --- 公共配置接口 ---
    void setMaxRipples(uint8_t count);
    void setSpeed(float speed);
    void setThickness(float thickness);
    void setSpawnInterval(float seconds);
    void setMaxRadius(float radius);
    void setRandomOrigin(bool random); 
    void setSaturation(float saturation);
    void setBrightness(float brightness); // ***** 新增接口 ***** (0.0 to 1.0)

private:
    struct Ripple
    {
        bool isActive;
        float originX, originY;
        unsigned long startTimeMs;
        float hue;
    };

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 8;
    uint8_t _matrixHeight = 8;

    Ripple* _ripples = nullptr;
    uint8_t _nextRippleIndex = 0;
    unsigned long _lastAutoRippleTimeMs = 0;

    // 私有配置参数
    uint8_t _maxRipples = 5;
    float _speed = 3.0f;
    float _maxRadius; 
    float _thickness = 1.8f;
    float _spawnIntervalS = 2.0f;
    float _saturation = 1.0f;
    bool _randomOrigin = false;
    float _brightness = 1.0f; // ***** 新增亮度控制变量 ***** (默认100%)
};

#endif // RIPPLE_EFFECT_H