#ifndef RIPPLE_EFFECT_H
#define RIPPLE_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class RippleEffect
{
public:
    struct Parameters {
        uint8_t maxRipples;
        float speed;
        float thickness;
        float spawnIntervalS;
        float maxRadius;
        bool randomOrigin;
        float saturation;
        float baseBrightness; // 0.0 to 1.0
        const char* prePara; // 新增字段，用于标识当前预设
    };

    static const Parameters WaterDropPreset;
    static const Parameters EnergyPulsePreset;

    RippleEffect();
    ~RippleEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8;
        _matrixHeight = 8;

        // Apply initial parameters
        setParameters(_params);

        _lastAutoRippleTimeMs = millis();
    }

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName); // 新增预设切换方法

private:
    struct Ripple {
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

    Parameters _params;
};

#endif // RIPPLE_EFFECT_H