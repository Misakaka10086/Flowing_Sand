#ifndef LAVA_LAMP_EFFECT_H
#define LAVA_LAMP_EFFECT_H

#include <NeoPixelBus.h>
#include "../../../include/TransitionUtils.h" // For DEFAULT_TRANSITION_DURATION_MS

class LavaLampEffect {
private:
    struct Metaball {
        float x, y;
        float vx, vy;
        float radius;
    };

public:
    struct Parameters {
        uint8_t numBlobs;
        float threshold;      // 能量阈值
        float baseSpeed;
        float baseBrightness;
        const char* baseColor; // 基础颜色, 16进制格式 (e.g., "#FF0000")
        float hueRange;       // 颜色从边缘到核心的变化范围
        const char* prePara;
    };

    static const Parameters ClassicLavaPreset;
    static const Parameters MercuryPreset;

    LavaLampEffect();
    ~LavaLampEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight);

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName);

private:
    void initBlobs();
    // 辅助函数，将16进制颜色字符串转换为RGB分量
    void hexToRgb(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b);
    // 将矩阵坐标映射到LED索引
    int mapCoordinatesToIndex(int x, int y);

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 0;
    uint8_t _matrixHeight = 0;
    
    Metaball* _blobs = nullptr;
    Parameters _activeParams;
    Parameters _targetParams;
    Parameters _oldParams;

    bool _effectInTransition;
    unsigned long _effectTransitionStartTimeMs;
    unsigned long _effectTransitionDurationMs;
    // 存储从 baseColor 解析出的H值，以提高性能
    float _activeInternalBaseHue = 0.0f;
    float _targetInternalBaseHue = 0.0f;
    float _oldInternalBaseHue = 0.0f;
    
    unsigned long _lastUpdateTime = 0;
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void LavaLampEffect::Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;
    // 确保在设置参数前初始化
    if (_blobs != nullptr) delete[] _blobs;
    _blobs = new Metaball[_activeParams.numBlobs];
    initBlobs();
    _lastUpdateTime = millis();
}

#endif // LAVA_LAMP_EFFECT_H