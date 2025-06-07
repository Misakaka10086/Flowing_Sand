#include "ZenLightsEffect.h"
#include <ArduinoJson.h>
// ***** 3. 在.cpp文件中定义和初始化静态预设 *****
const ZenLightsEffect::Parameters ZenLightsEffect::ZenPreset = {
    .maxActiveLeds = 8,
    .minDurationMs = 4000,
    .maxDurationMs = 8000,
    .minPeakBrightness = 0.1f,
    .maxPeakBrightness = 0.4f,
    .baseBrightness = 1.0f,
    .hueMin = 0.55f, // 淡蓝色
    .hueMax = 0.65f, // 偏青色
    .saturation = 0.6f,
    .spawnIntervalMs = 300};

const ZenLightsEffect::Parameters ZenLightsEffect::FireflyPreset = {
    .maxActiveLeds = 12,
    .minDurationMs = 1500,
    .maxDurationMs = 3500,
    .minPeakBrightness = 0.4f,
    .maxPeakBrightness = 0.9f,
    .baseBrightness = 1.0f,
    .hueMin = 0.12f, // 琥珀色
    .hueMax = 0.16f, // 黄色
    .saturation = 1.0f,
    .spawnIntervalMs = 150};

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
    _params.maxActiveLeds = doc["maxActiveLeds"] | _params.maxActiveLeds;
    _params.minDurationMs = doc["minDurationMs"] | _params.minDurationMs;
    _params.maxDurationMs = doc["maxDurationMs"] | _params.maxDurationMs;
    _params.minPeakBrightness = doc["minPeakBrightness"] | _params.minPeakBrightness;
    _params.maxPeakBrightness = doc["maxPeakBrightness"] | _params.maxPeakBrightness;
    _params.baseBrightness = doc["baseBrightness"] | _params.baseBrightness;
    _params.hueMin = doc["hueMin"] | _params.hueMin;
    _params.hueMax = doc["hueMax"] | _params.hueMax;
    _params.saturation = doc["saturation"] | _params.saturation;
    _params.spawnIntervalMs = doc["spawnIntervalMs"] | _params.spawnIntervalMs;

    Serial.println("ZenLightsEffect parameters updated via JSON.");
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