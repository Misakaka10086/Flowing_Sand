#ifndef CODE_RAIN_EFFECT_H
#define CODE_RAIN_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class CodeRainEffect
{
public:
    struct Parameters {
        float minSpeed;
        float maxSpeed;
        int minStreamLength;
        int maxStreamLength;
        float spawnProbability; // 0.0 to 1.0
        unsigned long minSpawnCooldownMs;
        unsigned long maxSpawnCooldownMs;
        float baseHue; // 0.0 to 1.0
        float hueVariation;
        float saturation;
        uint8_t baseBrightness; // 0-255, for HSB conversion
    };

    static const Parameters ClassicMatrixPreset;
    static const Parameters FastGlitchPreset;

    CodeRainEffect();
    ~CodeRainEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8;
        _matrixHeight = 8;
        
        // Apply initial parameters
        setParameters(_params);

        _lastFrameTimeMs = millis();
    }

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);

private:
    struct CodeStream {
        bool isActive;
        float currentY;
        float speed;
        int length;
        unsigned long spawnCooldownMs;
        unsigned long lastActivityTimeMs;
        float hue;
    };

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 8;
    uint8_t _matrixHeight = 8;

    CodeStream* _codeStreams = nullptr;
    unsigned long _lastFrameTimeMs = 0;

    Parameters _params;
};

#endif // CODE_RAIN_EFFECT_H