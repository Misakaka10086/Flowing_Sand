#ifndef ZEN_LIGHTS_EFFECT_H
#define ZEN_LIGHTS_EFFECT_H

#include <NeoPixelBus.h>
#include <math.h>

class ZenLightsEffect
{
public:
    // ***** 1. 定义公共的参数结构体 *****
    struct Parameters {
        int maxActiveLeds;
        unsigned long minDurationMs;
        unsigned long maxDurationMs;
        float minPeakBrightness; // 0.0 - 1.0
        float maxPeakBrightness; // 0.0 - 1.0
        float baseBrightness;    // 0.0 - 1.0, 控制整体亮度级别
        float hueMin;            // 0.0 - 1.0
        float hueMax;            // 0.0 - 1.0
        float saturation;        // 0.0 - 1.0
        unsigned long spawnIntervalMs;
        const char* prePara;     // 新增字段，用于标识当前预设
    };

    // ***** 2. 声明静态的预设 *****
    static const Parameters ZenPreset;      // 预设1: 宁静的蓝绿色
    static const Parameters FireflyPreset;  // 预设2: 温暖的萤火虫

    // 构造函数和析构函数
    ZenLightsEffect();
    ~ZenLightsEffect();

    // 初始化方法
    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight);

    // 主更新函数
    void Update();

    // --- 新的公共配置接口 ---
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName); // 新增预设切换方法

    // (可选) 保留旧的单个设置接口，或者移除它们以强制使用参数结构体
    // void setMaxActiveLeds(int count);
    // ...

private:
    struct LedEffectState {
        bool isActive;
        unsigned long startTimeMs;
        unsigned long durationMs;
        float peakBrightnessFactor; 
        float hue;                  
    };

    // 将矩阵坐标映射到LED索引
    int mapCoordinatesToIndex(int x, int y);

    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 0;
    uint8_t _matrixHeight = 0;
    LedEffectState* _ledStates = nullptr;
    unsigned long _lastAttemptTimeMs = 0;

    // 私有成员变量现在用来存储当前激活的参数
    Parameters _params; // 用一个参数对象来存储所有配置

    // 私有辅助函数
    int countActiveLeds();
    void tryActivateNewLed();
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void ZenLightsEffect::Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;
    if (_ledStates != nullptr) {
        delete[] _ledStates;
    }
    _ledStates = new LedEffectState[_numLeds];
    for (uint16_t i = 0; i < _numLeds; i++) {
        _ledStates[i].isActive = false;
    }
    _lastAttemptTimeMs = millis();
}

#endif // ZEN_LIGHTS_EFFECT_H