#ifndef CODE_RAIN_EFFECT_H
#define CODE_RAIN_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>
#include "../../../include/TransitionUtils.h"

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
        float baseBrightness; // 0-1, for HSB conversion
        const char* prePara; // 新增字段，用于标识当前预设
    };

    static const Parameters ClassicMatrixPreset;
    static const Parameters FastGlitchPreset;

    CodeRainEffect();
    ~CodeRainEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight);

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName);

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

    // 将矩阵坐标映射到LED索引
    int mapCoordinatesToIndex(int x, int y);

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 0;
    uint8_t _matrixHeight = 0;

    CodeStream* _codeStreams = nullptr;
    unsigned long _lastFrameTimeMs = 0;

    Parameters _activeParams;
    Parameters _targetParams;
    Parameters _oldParams;

    bool _effectInTransition;
    unsigned long _effectTransitionStartTimeMs;
    unsigned long _effectTransitionDurationMs;
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void CodeRainEffect::Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;
    
    // Apply initial parameters
    setParameters(ClassicMatrixPreset);

    _lastFrameTimeMs = millis();
}

#endif // CODE_RAIN_EFFECT_H