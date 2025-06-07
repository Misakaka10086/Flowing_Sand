#include "ZenLightsEffect.h"
#include <ArduinoJson.h>
// ***** 3. 在.cpp文件中定义和初始化静态预设 *****
const ZenLightsEffect::Parameters ZenLightsEffect::ZenPreset = {
    .maxActiveLeds = 5,
    .minDurationMs = 2000,
    .maxDurationMs = 4000,
    .minPeakBrightness = 0.3f,
    .maxPeakBrightness = 0.7f,
    .baseBrightness = 0.5f,
    .hueMin = 0.5f,  // 蓝绿色范围
    .hueMax = 0.6f,
    .saturation = 0.8f,
    .spawnIntervalMs = 500,
    .prePara = "Zen"
};

const ZenLightsEffect::Parameters ZenLightsEffect::FireflyPreset = {
    .maxActiveLeds = 8,
    .minDurationMs = 1000,
    .maxDurationMs = 3000,
    .minPeakBrightness = 0.4f,
    .maxPeakBrightness = 0.9f,
    .baseBrightness = 0.7f,
    .hueMin = 0.1f,  // 温暖的黄色范围
    .hueMax = 0.2f,
    .saturation = 0.9f,
    .spawnIntervalMs = 300,
    .prePara = "Firefly"
};

ZenLightsEffect::ZenLightsEffect()
{
    _ledStates = nullptr;
    _strip = nullptr;
    // 构造时加载默认预设
    setParameters(ZenPreset);
}

ZenLightsEffect::~ZenLightsEffect()
{
    if (_ledStates != nullptr)
    {
        delete[] _ledStates;
    }
}

// ***** 4. 实现新的 setParameters 方法 *****
void ZenLightsEffect::setParameters(const Parameters &params)
{
    _params = params;
}

void ZenLightsEffect::setParameters(const char *jsonParams)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error)
    {
        Serial.println("ZenLightsEffect::setParameters failed to parse JSON: " + String(error.c_str()));
        return;
    }

    // // 对于最大活动灯珠数的改变，需要特别处理，因为它涉及到内存重新分配
    if (doc["maxActiveLeds"].is<int>()) {
        int newMaxActiveLeds = doc["maxActiveLeds"].as<int>();
        if (newMaxActiveLeds != _params.maxActiveLeds) {
            _params.maxActiveLeds = newMaxActiveLeds;
            if (_strip != nullptr) { // 确保已经Begin
                // 创建新的状态数组
                LedEffectState* newLedStates = new LedEffectState[_numLeds];
                
                // 初始化新数组
                for (uint16_t i = 0; i < _numLeds; i++) {
                    newLedStates[i].isActive = false;
                }
                
                // 如果存在旧的状态数组，复制当前活动的LED状态
                if (_ledStates != nullptr) {
                    for (uint16_t i = 0; i < _numLeds; i++) {
                        if (_ledStates[i].isActive) {
                            // 保持当前活动的LED状态不变
                            newLedStates[i] = _ledStates[i];
                        }
                    }
                    delete[] _ledStates;
                }
                
                // 更新指针
                _ledStates = newLedStates;
                Serial.printf("ZenLightsEffect: Maximum active LEDs changed to %d\n", _params.maxActiveLeds);
            }
        }
    }
    // _params.maxActiveLeds = doc["maxActiveLeds"] | _params.maxActiveLeds;
    // 更新其他参数
    _params.minDurationMs = doc["minDurationMs"] | _params.minDurationMs;
    _params.maxDurationMs = doc["maxDurationMs"] | _params.maxDurationMs;
    _params.minPeakBrightness = doc["minPeakBrightness"] | _params.minPeakBrightness;
    _params.maxPeakBrightness = doc["maxPeakBrightness"] | _params.maxPeakBrightness;
    _params.baseBrightness = doc["baseBrightness"] | _params.baseBrightness;
    _params.hueMin = doc["hueMin"] | _params.hueMin;
    _params.hueMax = doc["hueMax"] | _params.hueMax;
    _params.saturation = doc["saturation"] | _params.saturation;
    _params.spawnIntervalMs = doc["spawnIntervalMs"] | _params.spawnIntervalMs;
    
    // 如果JSON中包含prePara字段，则更新它
    if (doc["prePara"].is<String>()) {
        const char* newPrePara = doc["prePara"].as<String>().c_str();
        if (strcmp(newPrePara, "Zen") == 0) {
            _params.prePara = ZenPreset.prePara;
        } else if (strcmp(newPrePara, "Firefly") == 0) {
            _params.prePara = FireflyPreset.prePara;
        }
    }
    
    Serial.println("ZenLightsEffect parameters updated via JSON.");
}

void ZenLightsEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "next") == 0) {
        // 使用prePara字段来判断当前预设
        if (strcmp(_params.prePara, "Zen") == 0) {
            setParameters(FireflyPreset);
            Serial.println("Switched to FireflyPreset");
        } else {
            setParameters(ZenPreset);
            Serial.println("Switched to ZenPreset");
        }
    } else if (strcmp(presetName, "Zen") == 0) {
        setParameters(ZenPreset);
        Serial.println("Switched to ZenPreset");
    } else if (strcmp(presetName, "Firefly") == 0) {
        setParameters(FireflyPreset);
        Serial.println("Switched to FireflyPreset");
    } else {
        Serial.println("Unknown preset name: " + String(presetName));
    }
}

void ZenLightsEffect::Update()
{
    if (_strip == nullptr || _ledStates == nullptr)
        return;

    unsigned long currentTimeMs = millis();

    // 1. 尝试激活新的灯珠
    if (currentTimeMs - _lastAttemptTimeMs >= _params.spawnIntervalMs)
    {
        _lastAttemptTimeMs = currentTimeMs;
        tryActivateNewLed();
    }

    // 2. 更新并渲染所有灯珠
    for (uint16_t i = 0; i < _numLeds; ++i)
    {
        if (_ledStates[i].isActive)
        {
            unsigned long elapsedTime = currentTimeMs - _ledStates[i].startTimeMs;

            if (elapsedTime >= _ledStates[i].durationMs)
            {
                _ledStates[i].isActive = false;
                _strip->SetPixelColor(i, RgbColor(0, 0, 0));
                continue;
            }

            float progress = (float)elapsedTime / _ledStates[i].durationMs;
            float currentCycleBrightness;

            if (progress < 0.5f)
            {
                currentCycleBrightness = progress * 2.0f;
            }
            else
            {
                currentCycleBrightness = (1.0f - progress) * 2.0f;
            }

            currentCycleBrightness *= _ledStates[i].peakBrightnessFactor;
            currentCycleBrightness = constrain(currentCycleBrightness, 0.0f, 1.0f);

            HsbColor hsbColor(_ledStates[i].hue, _params.saturation, currentCycleBrightness);
            _strip->SetPixelColor(i, hsbColor);
        }
        else
        {
            _strip->SetPixelColor(i, RgbColor(0, 0, 0));
        }
    }
}

// --- 私有辅助函数实现 ---

int ZenLightsEffect::countActiveLeds()
{
    int count = 0;
    for (uint16_t i = 0; i < _numLeds; ++i)
    {
        if (_ledStates[i].isActive)
        {
            count++;
        }
    }
    return count;
}

void ZenLightsEffect::tryActivateNewLed()
{
    if (countActiveLeds() >= _params.maxActiveLeds)
    {
        return;
    }

    int candidateLed = random(_numLeds);
    int attempts = 0;
    while (_ledStates[candidateLed].isActive && attempts < _numLeds * 2)
    {
        candidateLed = random(_numLeds);
        attempts++;
    }

    if (!_ledStates[candidateLed].isActive)
    {
        _ledStates[candidateLed].isActive = true;
        _ledStates[candidateLed].startTimeMs = millis();
        _ledStates[candidateLed].durationMs = random(_params.minDurationMs, _params.maxDurationMs + 1);
        _ledStates[candidateLed].peakBrightnessFactor = _params.baseBrightness * random(_params.minPeakBrightness * 100, _params.maxPeakBrightness * 100 + 1) / 100.0f;
        _ledStates[candidateLed].hue = random(_params.hueMin * 1000, _params.hueMax * 1000 + 1) / 1000.0f;
    }
}