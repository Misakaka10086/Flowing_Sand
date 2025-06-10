#include "CodeRainEffect.h"
#include <ArduinoJson.h>
// 定义和初始化静态预设
const CodeRainEffect::Parameters CodeRainEffect::ClassicMatrixPreset = {
    .minSpeed = 12.0f,
    .maxSpeed = 20.0f,
    .minStreamLength = 3,
    .maxStreamLength = 7,
    .spawnProbability = 0.15f,
    .minSpawnCooldownMs = 100,
    .maxSpawnCooldownMs = 400,
    .baseHue = 0.33f, // Green
    .hueVariation = 0.05f,
    .saturation = 1.0f,
    .baseBrightness = 0.8f,
    .prePara = "ClassicMatrix"
};

const CodeRainEffect::Parameters CodeRainEffect::FastGlitchPreset = {
    .minSpeed = 25.0f,
    .maxSpeed = 50.0f, // 非常快
    .minStreamLength = 2,
    .maxStreamLength = 5, // 流更短
    .spawnProbability = 0.3f, // 更密集
    .minSpawnCooldownMs = 20,
    .maxSpawnCooldownMs = 100,
    .baseHue = 0.0f, // 红色 "glitch"
    .hueVariation = 0.02f,
    .saturation = 1.0f,
    .baseBrightness = 1.0f,
    .prePara = "FastGlitch"
};


CodeRainEffect::CodeRainEffect() {
    _codeStreams = nullptr;
    _strip = nullptr;
    setParameters(ClassicMatrixPreset); // 构造时加载默认预设
}

CodeRainEffect::~CodeRainEffect() {
    if (_codeStreams != nullptr) {
        delete[] _codeStreams;
    }
}

void CodeRainEffect::setParameters(const Parameters& params) {
    _params = params;
    // CodeRainEffect 的流数量总是等于矩阵宽度，所以不需要重新分配
    if (_codeStreams == nullptr && _matrixWidth > 0) {
        _codeStreams = new CodeStream[_matrixWidth];
        for (uint8_t i = 0; i < _matrixWidth; ++i) {
            _codeStreams[i].isActive = false;
            // 初始化冷却时间，确保第一次能生成
            _codeStreams[i].lastActivityTimeMs = millis() - random(_params.minSpawnCooldownMs, _params.maxSpawnCooldownMs);
            _codeStreams[i].spawnCooldownMs = random(_params.minSpawnCooldownMs, _params.maxSpawnCooldownMs);
        }
    } else if (_codeStreams != nullptr) { // If params are changed at runtime
        for (uint8_t i = 0; i < _matrixWidth; ++i) { // Reset cooldowns based on new params
            _codeStreams[i].spawnCooldownMs = random(_params.minSpawnCooldownMs, _params.maxSpawnCooldownMs);
        }
    }
}

void CodeRainEffect::setParameters(const char* jsonParams) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.println("CodeRainEffect::setParameters failed to parse JSON: " + String(error.c_str()));
        return;
    }
    _params.minSpeed = doc["minSpeed"] | _params.minSpeed;
    _params.maxSpeed = doc["maxSpeed"] | _params.maxSpeed;
    _params.minStreamLength = doc["minStreamLength"] | _params.minStreamLength;
    _params.maxStreamLength = doc["maxStreamLength"] | _params.maxStreamLength;
    _params.spawnProbability = doc["spawnProbability"] | _params.spawnProbability;
    _params.minSpawnCooldownMs = doc["minSpawnCooldownMs"] | _params.minSpawnCooldownMs;    
    _params.maxSpawnCooldownMs = doc["maxSpawnCooldownMs"] | _params.maxSpawnCooldownMs;
    _params.baseHue = doc["baseHue"] | _params.baseHue;
    _params.hueVariation = doc["hueVariation"] | _params.hueVariation;
    _params.saturation = doc["saturation"] | _params.saturation;
    _params.baseBrightness = doc["baseBrightness"] | _params.baseBrightness;
    
    // 如果JSON中包含prePara字段，则更新它
    if (doc["prePara"].is<String>()) {
        const char* newPrePara = doc["prePara"].as<String>().c_str();
        if (strcmp(newPrePara, "ClassicMatrix") == 0) {
            _params.prePara = ClassicMatrixPreset.prePara;
        } else if (strcmp(newPrePara, "FastGlitch") == 0) {
            _params.prePara = FastGlitchPreset.prePara;
        }
    }
    
    Serial.println("CodeRainEffect parameters updated via JSON.");
}

void CodeRainEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "next") == 0) {
        // 使用prePara字段来判断当前预设
        if (strcmp(_params.prePara, "ClassicMatrix") == 0) {
            setParameters(FastGlitchPreset);
            Serial.println("Switched to FastGlitchPreset");
        } else {
            setParameters(ClassicMatrixPreset);
            Serial.println("Switched to ClassicMatrixPreset");
        }
    } else if (strcmp(presetName, "ClassicMatrix") == 0) {
        setParameters(ClassicMatrixPreset);
        Serial.println("Switched to ClassicMatrixPreset");
    } else if (strcmp(presetName, "FastGlitch") == 0) {
        setParameters(FastGlitchPreset);
        Serial.println("Switched to FastGlitchPreset");
    } else {
        Serial.println("Unknown preset name: " + String(presetName));
    }
}

int CodeRainEffect::mapCoordinatesToIndex(int x, int y) {
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

void CodeRainEffect::Update() {
    if (_strip == nullptr || _codeStreams == nullptr) return;

    unsigned long currentTimeMs = millis();
    float deltaTimeS = (currentTimeMs - _lastFrameTimeMs) / 1000.0f;
    if (deltaTimeS <= 0.001f) {
        deltaTimeS = 0.001f;
    }
    _lastFrameTimeMs = currentTimeMs;

    _strip->ClearTo(RgbColor(0, 0, 0));

    // Update and generate streams using _params
    for (int x = 0; x < _matrixWidth; ++x) {
        if (_codeStreams[x].isActive) {
            _codeStreams[x].currentY += _codeStreams[x].speed * deltaTimeS;
            if (_codeStreams[x].currentY - _codeStreams[x].length >= _matrixHeight) {
                _codeStreams[x].isActive = false;
                _codeStreams[x].lastActivityTimeMs = currentTimeMs;
                _codeStreams[x].spawnCooldownMs = random(_params.minSpawnCooldownMs, _params.maxSpawnCooldownMs);
            }
        } else {
            if (currentTimeMs - _codeStreams[x].lastActivityTimeMs >= _codeStreams[x].spawnCooldownMs) {
                if (random(1000) / 1000.0f < _params.spawnProbability) {
                    _codeStreams[x].isActive = true;
                    _codeStreams[x].currentY = -random(0, _matrixHeight * 2);
                    _codeStreams[x].speed = random(_params.minSpeed * 100, _params.maxSpeed * 100) / 100.0f;
                    _codeStreams[x].length = random(_params.minStreamLength, _params.maxStreamLength + 1);
                    _codeStreams[x].hue = _params.baseHue + (random(-_params.hueVariation * 100, _params.hueVariation * 100) / 100.0f);
                    if (_codeStreams[x].hue < 0.0f) _codeStreams[x].hue += 1.0f;
                    if (_codeStreams[x].hue > 1.0f) _codeStreams[x].hue -= 1.0f;
                    _codeStreams[x].lastActivityTimeMs = currentTimeMs;
                }
            }
        }
    }

    // Render streams
    for (int x = 0; x < _matrixWidth; ++x) {
        if (_codeStreams[x].isActive) {
            for (int l = 0; l < _codeStreams[x].length; ++l) {
                int charY = floor(_codeStreams[x].currentY - 1 - l);
                if (charY >= 0 && charY < _matrixHeight) {
                    float brightnessFactor;
                    if (l == 0) {
                        brightnessFactor = 1.0f;
                    } else {
                        brightnessFactor = 0.8f * (1.0f - (float)l / (_codeStreams[x].length > 1 ? _codeStreams[x].length - 1 : 1));
                        brightnessFactor = constrain(brightnessFactor, 0.05f, 0.8f);
                    }
                    if (l > 0 && random(100) < 10) {
                        brightnessFactor *= (random(70, 101) / 100.0f);
                    }
                    
                    HsbColor hsbColor(_codeStreams[x].hue, _params.saturation, brightnessFactor * _params.baseBrightness);
                    int ledIndex = mapCoordinatesToIndex(x, charY);

                    if (ledIndex >= 0 && ledIndex < _numLeds) {
                        _strip->SetPixelColor(ledIndex, hsbColor);
                    }
                }
            }
        }
    }
}