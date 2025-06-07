#include "CodeRainEffect.h"

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
    .baseBrightness = 220
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
    .baseBrightness = 255
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
                    
                    HsbColor hsbColor(_codeStreams[x].hue, _params.saturation, brightnessFactor * (_params.baseBrightness / 255.0f));
                    int logicalDrawY = _matrixHeight - 1 - charY;
                    int ledIndex = logicalDrawY * _matrixWidth + (_matrixWidth - 1 - x);

                    if (ledIndex >= 0 && ledIndex < _numLeds) {
                        _strip->SetPixelColor(ledIndex, hsbColor);
                    }
                }
            }
        }
    }
}