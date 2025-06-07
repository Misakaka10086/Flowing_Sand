#ifndef CODE_RAIN_EFFECT_H
#define CODE_RAIN_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class CodeRainEffect
{
public:
    CodeRainEffect();
    ~CodeRainEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip)
    {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        _matrixWidth = 8; // Assuming 8x8 matrix
        _matrixHeight = 8;

        if (_codeStreams != nullptr) {
            delete[] _codeStreams;
        }
        _codeStreams = new CodeStream[_matrixWidth];

        for (uint8_t i = 0; i < _matrixWidth; ++i) {
            _codeStreams[i].isActive = false;
            _codeStreams[i].lastActivityTimeMs = millis() - random(_minSpawnCooldownMs, _maxSpawnCooldownMs);
            _codeStreams[i].spawnCooldownMs = random(_minSpawnCooldownMs, _maxSpawnCooldownMs);
        }
        _lastFrameTimeMs = millis();
    }

    void Update();

    // --- 公共配置接口 ---
    void setSpeedRange(float min, float max);
    void setLengthRange(int min, int max);
    void setSpawnProbability(float probability); // 0.0 to 1.0
    void setSpawnCooldownRange(unsigned long minMs, unsigned long maxMs);
    void setBaseHue(float hue); // 0.0 to 1.0
    void setHueVariation(float variation);
    void setSaturation(float saturation);
    void setBaseBrightness(uint8_t brightness); // 0-255, for HSB conversion

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

    // 私有配置参数
    float _minSpeed = 12.0f;
    float _maxSpeed = 36.0f;
    int _minStreamLength = 3;
    int _maxStreamLength = 7;
    float _spawnProbability = 0.15f;
    unsigned long _minSpawnCooldownMs = 50;
    unsigned long _maxSpawnCooldownMs = 200;
    float _baseHue = 0.33f;
    float _hueVariation = 0.05f;
    float _saturation = 1.0f;
    uint8_t _baseBrightness = 200; // Used for HSB conversion, not overall strip brightness
};

#endif // CODE_RAIN_EFFECT_H