#include "ZenLightsEffect.h"

ZenLightsEffect::ZenLightsEffect() {
    // 构造函数中可以初始化一些变量，但主要逻辑在Begin中
    // 确保指针在开始时是安全的
    _ledStates = nullptr;
    _strip = nullptr;
}

ZenLightsEffect::~ZenLightsEffect() {
    // 析构函数：释放动态分配的内存，防止内存泄漏
    if (_ledStates != nullptr) {
        delete[] _ledStates;
    }
}

void ZenLightsEffect::Update() {
    if (_strip == nullptr || _ledStates == nullptr) return; // 如果未初始化，则不执行任何操作

    unsigned long currentTimeMs = millis();

    // 1. 尝试激活新的灯珠
    if (currentTimeMs - _lastAttemptTimeMs >= _spawnIntervalMs) {
        _lastAttemptTimeMs = currentTimeMs;
        tryActivateNewLed();
    }

    // 2. 更新并渲染所有灯珠
    for (uint16_t i = 0; i < _numLeds; ++i) {
        if (_ledStates[i].isActive) {
            unsigned long elapsedTime = currentTimeMs - _ledStates[i].startTimeMs;

            if (elapsedTime >= _ledStates[i].durationMs) {
                _ledStates[i].isActive = false;
                _strip->SetPixelColor(i, RgbColor(0, 0, 0)); 
                continue;
            }

            float progress = (float)elapsedTime / _ledStates[i].durationMs;
            float currentCycleBrightness;

            // 三角波计算
            if (progress < 0.5f) {
                currentCycleBrightness = progress * 2.0f;
            } else {
                currentCycleBrightness = (1.0f - progress) * 2.0f;
            }

            currentCycleBrightness *= _ledStates[i].peakBrightnessFactor;
            currentCycleBrightness = constrain(currentCycleBrightness, 0.0f, 1.0f);

            HsbColor hsbColor(_ledStates[i].hue, _saturation, currentCycleBrightness);
            _strip->SetPixelColor(i, hsbColor); 

        } else {
             _strip->SetPixelColor(i, RgbColor(0, 0, 0)); 
        }
    }
    // 注意: strip.Show() 应该在主循环中调用，以便可以组合多个效果
}

// --- 私有辅助函数实现 ---

int ZenLightsEffect::countActiveLeds() {
    int count = 0;
    for (uint16_t i = 0; i < _numLeds; ++i) {
        if (_ledStates[i].isActive) {
            count++;
        }
    }
    return count;
}

void ZenLightsEffect::tryActivateNewLed() {
    if (countActiveLeds() >= _maxActiveLeds) {
        return; 
    }

    int candidateLed = random(_numLeds);
    int attempts = 0;
    while (_ledStates[candidateLed].isActive && attempts < _numLeds * 2) { 
        candidateLed = random(_numLeds);
        attempts++;
    }

    if (!_ledStates[candidateLed].isActive) {
        _ledStates[candidateLed].isActive = true;
        _ledStates[candidateLed].startTimeMs = millis();
        _ledStates[candidateLed].durationMs = random(_minDurationMs, _maxDurationMs + 1);
        _ledStates[candidateLed].peakBrightnessFactor = random(_minPeakBrightness * 100, _maxPeakBrightness * 100 + 1) / 100.0f;
        _ledStates[candidateLed].hue = random(_hueMin * 1000, _hueMax * 1000 + 1) / 1000.0f;
    }
}

// --- 公共配置接口实现 ---

void ZenLightsEffect::setMaxActiveLeds(int count) {
    _maxActiveLeds = count;
}

void ZenLightsEffect::setDurationRange(unsigned long minMs, unsigned long maxMs) {
    _minDurationMs = minMs;
    _maxDurationMs = maxMs;
}

void ZenLightsEffect::setBrightnessRange(float min, float max) {
    _minPeakBrightness = constrain(min, 0.0f, 1.0f);
    _maxPeakBrightness = constrain(max, 0.0f, 1.0f);
}

void ZenLightsEffect::setHueRange(float min, float max) {
    _hueMin = min;
    _hueMax = max;
}

void ZenLightsEffect::setSaturation(float saturation) {
    _saturation = constrain(saturation, 0.0f, 1.0f);
}

void ZenLightsEffect::setSpawnInterval(unsigned long intervalMs) {
    _spawnIntervalMs = intervalMs;
}