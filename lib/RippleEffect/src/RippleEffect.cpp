#include "RippleEffect.h"
#include <ArduinoJson.h>
// 定义和初始化静态预设
const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5,
    .speed = 3.0f,
    .thickness = 1.8f,
    .spawnIntervalS = 2.0f,
    .maxRadius = 8 * 1.2f,
    .randomOrigin = false, // 中心水滴
    .saturation = 1.0f,
    .baseBrightness = 0.8f,
    .prePara = "WaterDrop"
};

const RippleEffect::Parameters RippleEffect::EnergyPulsePreset = {
    .maxRipples = 8,
    .speed = 6.0f, // 更快
    .thickness = 1.2f, // 更细
    .spawnIntervalS = 0.5f, // 更频繁
    .maxRadius = 8 * 1.5f,
    .randomOrigin = true, // 随机脉冲
    .saturation = 0.7f,
    .baseBrightness = 0.2f,
    .prePara = "EnergyPulse"
};

RippleEffect::RippleEffect() {
    _ripples = nullptr;
    _strip = nullptr;
    setParameters(WaterDropPreset); // 构造时加载默认预设
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

void RippleEffect::setParameters(const Parameters& params) {
    bool numRipplesChanged = (_params.maxRipples != params.maxRipples);
    _params = params;

    // 如果最大波纹数改变，需要重新分配内存
    if (numRipplesChanged || _ripples == nullptr) {
        if (_ripples != nullptr) delete[] _ripples;
        _ripples = new Ripple[_params.maxRipples];
        for (uint8_t i = 0; i < _params.maxRipples; ++i) {
            _ripples[i].isActive = false;
        }
        _nextRippleIndex = 0;
    }
}

void RippleEffect::setParameters(const char* jsonParams) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.println("RippleEffect::setParameters failed to parse JSON: " + String(error.c_str()));
        return;
    }

    // 对于最大波纹数的改变，需要特别处理，因为它涉及到内存重新分配
    if (doc["maxRipples"].is<uint8_t>()) {
        uint8_t newMaxRipples = doc["maxRipples"].as<uint8_t>();
        if (newMaxRipples != _params.maxRipples) {
            _params.maxRipples = newMaxRipples;
            if (_strip != nullptr) { // 确保已经Begin
                setParameters(_params); // 这会触发内存重新分配
                Serial.printf("RippleEffect: Maximum ripples changed to %d\n", _params.maxRipples);
            }
        }
    }

    // 更新其他参数
    _params.speed = doc["speed"] | _params.speed;
    _params.thickness = doc["thickness"] | _params.thickness;
    _params.spawnIntervalS = doc["spawnIntervalS"] | _params.spawnIntervalS;
    _params.maxRadius = doc["maxRadius"] | _params.maxRadius;
    _params.randomOrigin = doc["randomOrigin"] | _params.randomOrigin;
    _params.saturation = doc["saturation"] | _params.saturation;
    _params.baseBrightness = doc["baseBrightness"] | _params.baseBrightness;
    
    // 如果JSON中包含prePara字段，则更新它
    if (doc["prePara"].is<String>()) {
        const char* newPrePara = doc["prePara"].as<String>().c_str();
        if (strcmp(newPrePara, "WaterDrop") == 0) {
            _params.prePara = WaterDropPreset.prePara;
        } else if (strcmp(newPrePara, "EnergyPulse") == 0) {
            _params.prePara = EnergyPulsePreset.prePara;
        }
    }
    
    Serial.println("RippleEffect parameters updated via JSON.");
}

void RippleEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "next") == 0) {
        // 使用prePara字段来判断当前预设
        if (strcmp(_params.prePara, "WaterDrop") == 0) {
            setParameters(EnergyPulsePreset);
            Serial.println("Switched to EnergyPulsePreset");
        } else {
            setParameters(WaterDropPreset);
            Serial.println("Switched to WaterDropPreset");
        }
    } else if (strcmp(presetName, "WaterDrop") == 0) {
        setParameters(WaterDropPreset);
        Serial.println("Switched to WaterDropPreset");
    } else if (strcmp(presetName, "EnergyPulse") == 0) {
        setParameters(EnergyPulsePreset);
        Serial.println("Switched to EnergyPulsePreset");
    } else {
        Serial.println("Unknown preset name: " + String(presetName));
    }
}

void RippleEffect::Update() {
    if (_strip == nullptr || _ripples == nullptr) return;

    unsigned long currentTimeMs = millis();

    // Auto-create ripples
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_params.spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;

        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;

        if (_params.randomOrigin) {
            originX += random(-150, 151) / 100.0f;
            originY += random(-150, 151) / 100.0f;
            originX = constrain(originX, 0.5f, _matrixWidth - 0.5f);
            originY = constrain(originY, 0.5f, _matrixHeight - 0.5f);
        }

        float randomHue = random(0, 1000) / 1000.0f;
        
        _ripples[_nextRippleIndex].isActive = true;
        _ripples[_nextRippleIndex].originX = originX;
        _ripples[_nextRippleIndex].originY = originY;
        _ripples[_nextRippleIndex].startTimeMs = currentTimeMs;
        _ripples[_nextRippleIndex].hue = randomHue;
        _nextRippleIndex = (_nextRippleIndex + 1) % _params.maxRipples;
    }

    // Render LED matrix
    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int j_pixel = 0; j_pixel < _matrixHeight; ++j_pixel) {
        for (int i_pixel = 0; i_pixel < _matrixWidth; ++i_pixel) {
            float pixelCenterX = i_pixel + 0.5f;
            float pixelCenterY = j_pixel + 0.5f;

            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0);

            for (int r_idx = 0; r_idx < _params.maxRipples; ++r_idx) {
                if (_ripples[r_idx].isActive) {
                    unsigned long rippleStartTime = _ripples[r_idx].startTimeMs;
                    float elapsedTimeS = (float)(currentTimeMs - rippleStartTime) / 1000.0f;
                    float currentRadius = elapsedTimeS * _params.speed;

                    if (currentRadius > _params.maxRadius + _params.thickness / 2.0f) {
                        _ripples[r_idx].isActive = false;
                        continue;
                    }

                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) + pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    float distToRippleEdge = abs(distToOrigin - currentRadius);

                    if (distToRippleEdge < _params.thickness / 2.0f) {
                        float intensityFactor = cos((distToRippleEdge / (_params.thickness / 2.0f)) * (PI / 2.0f));
                        intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);
                        if (intensityFactor > maxIntensityForThisPixel) {
                            maxIntensityForThisPixel = intensityFactor;
                            float finalBrightness = maxIntensityForThisPixel * _params.baseBrightness;
                            colorForThisPixel = HsbColor(_ripples[r_idx].hue, _params.saturation, finalBrightness);
                        }
                    }
                }
            }

            if (maxIntensityForThisPixel > 0.0f) {
                int ledIndex = j_pixel * _matrixWidth + (_matrixWidth - 1 - i_pixel);
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    _strip->SetPixelColor(ledIndex, colorForThisPixel);
                }
            }
        }
    }
}