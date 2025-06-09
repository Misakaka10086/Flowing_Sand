#ifndef LAVA_LAMP_EFFECT_H
#define LAVA_LAMP_EFFECT_H

#include <NeoPixelBus.h>

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
    void Begin(T_NeoPixelBus& strip);

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName);

private:
    void initBlobs();
    // 辅助函数，将16进制颜色字符串转换为RGB分量
    void hexToRgb(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b);

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint8_t _matrixWidth = 8;
    uint8_t _matrixHeight = 8;
    
    Metaball* _blobs = nullptr;
    Parameters _params;
    // 存储从 baseColor 解析出的H值，以提高性能
    float _internalBaseHue = 0.0f; 
    
    unsigned long _lastUpdateTime = 0;
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void LavaLampEffect::Begin(T_NeoPixelBus& strip) {
    _strip = &strip;
    // 确保在设置参数前初始化
    if (_blobs != nullptr) delete[] _blobs;
    _blobs = new Metaball[_params.numBlobs];
    initBlobs();
    _lastUpdateTime = millis();
}

#endif // LAVA_LAMP_EFFECT_H