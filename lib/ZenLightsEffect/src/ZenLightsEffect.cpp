#include "ZenLightsEffect.h"
#include <ArduinoJson.h>
#include "../../../include/DebugUtils.h"
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
    _activeParams = ZenPreset;
    _targetParams = ZenPreset;
    _oldParams = ZenPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    _ledStates = nullptr;
    _strip = nullptr;
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
    DEBUG_PRINTLN("ZenLightsEffect::setParameters(const Parameters&) called.");
    _oldParams = _activeParams;
    _targetParams = params;
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    DEBUG_PRINTLN("ZenLightsEffect transition started.");
}

void ZenLightsEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("ZenLightsEffect::setParameters(json) called [refactored].");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("ZenLightsEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc["maxActiveLeds"].is<int>()) newParams.maxActiveLeds = doc["maxActiveLeds"].as<int>();
    if (doc["minDurationMs"].is<unsigned long>()) newParams.minDurationMs = doc["minDurationMs"].as<unsigned long>();
    if (doc["maxDurationMs"].is<unsigned long>()) newParams.maxDurationMs = doc["maxDurationMs"].as<unsigned long>();
    if (doc["minPeakBrightness"].is<float>()) newParams.minPeakBrightness = doc["minPeakBrightness"].as<float>();
    if (doc["maxPeakBrightness"].is<float>()) newParams.maxPeakBrightness = doc["maxPeakBrightness"].as<float>();
    if (doc["baseBrightness"].is<float>()) newParams.baseBrightness = doc["baseBrightness"].as<float>();
    if (doc["hueMin"].is<float>()) newParams.hueMin = doc["hueMin"].as<float>();
    if (doc["hueMax"].is<float>()) newParams.hueMax = doc["hueMax"].as<float>();
    if (doc["saturation"].is<float>()) newParams.saturation = doc["saturation"].as<float>();
    if (doc["spawnIntervalMs"].is<unsigned long>()) newParams.spawnIntervalMs = doc["spawnIntervalMs"].as<unsigned long>();

    if (doc["prePara"].is<const char*>()) {
        const char* presetStr = doc["prePara"].as<const char*>();
        if (presetStr) {
            if (strcmp(presetStr, ZenPreset.prePara) == 0) {
                newParams.prePara = ZenPreset.prePara;
            } else if (strcmp(presetStr, FireflyPreset.prePara) == 0) {
                newParams.prePara = FireflyPreset.prePara;
            }
        }
    }
    
    // Call the struct version to handle all logic including transition setup
    setParameters(newParams);
}

void ZenLightsEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("ZenLightsEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, "next") == 0) {
        const char* currentEffectivePreset = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;
        if (currentEffectivePreset == nullptr) currentEffectivePreset = ZenPreset.prePara;

        if (strcmp(currentEffectivePreset, ZenPreset.prePara) == 0) {
            setParameters(FireflyPreset);
        } else {
            setParameters(ZenPreset);
        }
    } else if (strcmp(presetName, ZenPreset.prePara) == 0) {
        setParameters(ZenPreset);
    } else if (strcmp(presetName, FireflyPreset.prePara) == 0) {
        setParameters(FireflyPreset);
    } else {
        DEBUG_PRINTF("Unknown preset name in ZenLightsEffect::setPreset: %s\n", presetName);
    }
}

void ZenLightsEffect::Update()
{
    if (_effectInTransition) {
        unsigned long currentTimeMs_lerp = millis(); // Use a different name to avoid conflict
        unsigned long elapsedTime = currentTimeMs_lerp - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        _activeParams.maxActiveLeds = lerp(_oldParams.maxActiveLeds, _targetParams.maxActiveLeds, t);
        _activeParams.minDurationMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.minDurationMs), static_cast<int>(_targetParams.minDurationMs), t));
        _activeParams.maxDurationMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.maxDurationMs), static_cast<int>(_targetParams.maxDurationMs), t));
        _activeParams.minPeakBrightness = lerp(_oldParams.minPeakBrightness, _targetParams.minPeakBrightness, t);
        _activeParams.maxPeakBrightness = lerp(_oldParams.maxPeakBrightness, _targetParams.maxPeakBrightness, t);
        _activeParams.baseBrightness = lerp(_oldParams.baseBrightness, _targetParams.baseBrightness, t);
        _activeParams.hueMin = lerp(_oldParams.hueMin, _targetParams.hueMin, t);
        _activeParams.hueMax = lerp(_oldParams.hueMax, _targetParams.hueMax, t);
        _activeParams.saturation = lerp(_oldParams.saturation, _targetParams.saturation, t);
        _activeParams.spawnIntervalMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.spawnIntervalMs), static_cast<int>(_targetParams.spawnIntervalMs), t));

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams; // Ensure exact match at the end
            DEBUG_PRINTLN("ZenLightsEffect transition complete.");
        }
    }

    if (_strip == nullptr || _ledStates == nullptr)
        return;

    unsigned long currentTimeMs = millis();

    // 1. 尝试激活新的灯珠
    if (currentTimeMs - _lastAttemptTimeMs >= _activeParams.spawnIntervalMs)
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

            HsbColor hsbColor(_ledStates[i].hue, _activeParams.saturation, currentCycleBrightness);
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

int ZenLightsEffect::mapCoordinatesToIndex(int x, int y) {
    const int module_width = 8;
    const int module_height = 8;
    const int leds_per_module = module_width * module_height;
    int module_col = x / module_width;
    int module_row = y / module_height;
    int base_index;
    if (module_row == 1 && module_col == 1) base_index = 0;
    else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 1;
    else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 2;
    else base_index = leds_per_module * 3;
    int local_x = x % module_width;
    int local_y = y % module_height;
    int local_offset = (module_height - 1 - local_y) * module_width + (module_width - 1 - local_x);
    return base_index + local_offset;
}

void ZenLightsEffect::tryActivateNewLed()
{
    if (countActiveLeds() >= _activeParams.maxActiveLeds)
    {
        return;
    }

    // 随机选择一个坐标
    int x = random(_matrixWidth);
    int y = random(_matrixHeight);
    int candidateLed = mapCoordinatesToIndex(x, y);
    int attempts = 0;
    while (_ledStates[candidateLed].isActive && attempts < _numLeds * 2)
    {
        x = random(_matrixWidth);
        y = random(_matrixHeight);
        candidateLed = mapCoordinatesToIndex(x, y);
        attempts++;
    }

    if (!_ledStates[candidateLed].isActive)
    {
        _ledStates[candidateLed].isActive = true;
        _ledStates[candidateLed].startTimeMs = millis();
        _ledStates[candidateLed].durationMs = random(_activeParams.minDurationMs, _activeParams.maxDurationMs + 1);
        _ledStates[candidateLed].peakBrightnessFactor = _activeParams.baseBrightness * random(_activeParams.minPeakBrightness * 100, _activeParams.maxPeakBrightness * 100 + 1) / 100.0f;
        _ledStates[candidateLed].hue = random(_activeParams.hueMin * 1000, _activeParams.hueMax * 1000 + 1) / 1000.0f;
    }
}