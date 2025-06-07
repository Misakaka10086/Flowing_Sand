#include "CodeRainEffect.h"

CodeRainEffect::CodeRainEffect() {
    _codeStreams = nullptr;
    _strip = nullptr;
}

CodeRainEffect::~CodeRainEffect() {
    if (_codeStreams != nullptr) {
        delete[] _codeStreams;
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

    // Update and generate streams
    for (int x = 0; x < _matrixWidth; ++x) {
        if (_codeStreams[x].isActive) {
            _codeStreams[x].currentY += _codeStreams[x].speed * deltaTimeS;
            if (_codeStreams[x].currentY - _codeStreams[x].length >= _matrixHeight) {
                _codeStreams[x].isActive = false;
                _codeStreams[x].lastActivityTimeMs = currentTimeMs;
                _codeStreams[x].spawnCooldownMs = random(_minSpawnCooldownMs, _maxSpawnCooldownMs);
            }
        } else {
            if (currentTimeMs - _codeStreams[x].lastActivityTimeMs >= _codeStreams[x].spawnCooldownMs) {
                if (random(1000) / 1000.0f < _spawnProbability) {
                    _codeStreams[x].isActive = true;
                    _codeStreams[x].currentY = -random(0, _matrixHeight * 2);
                    _codeStreams[x].speed = random(_minSpeed * 100, _maxSpeed * 100) / 100.0f;
                    _codeStreams[x].length = random(_minStreamLength, _maxStreamLength + 1);
                    _codeStreams[x].hue = _baseHue + (random(-_hueVariation * 100, _hueVariation * 100) / 100.0f);
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
                    
                    HsbColor hsbColor(_codeStreams[x].hue, _saturation, brightnessFactor * (_baseBrightness / 255.0f));
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

// --- 公共配置接口实现 ---

void CodeRainEffect::setSpeedRange(float min, float max) { _minSpeed = min; _maxSpeed = max; }
void CodeRainEffect::setLengthRange(int min, int max) { 
    _minStreamLength = constrain(min, 1, _matrixHeight);
    _maxStreamLength = constrain(max, min, _matrixHeight);
}
void CodeRainEffect::setSpawnProbability(float probability) { _spawnProbability = constrain(probability, 0.0f, 1.0f); }
void CodeRainEffect::setSpawnCooldownRange(unsigned long minMs, unsigned long maxMs) { _minSpawnCooldownMs = minMs; _maxSpawnCooldownMs = maxMs; }
void CodeRainEffect::setBaseHue(float hue) { _baseHue = hue; }
void CodeRainEffect::setHueVariation(float variation) { _hueVariation = variation; }
void CodeRainEffect::setSaturation(float saturation) { _saturation = constrain(saturation, 0.0f, 1.0f); }
void CodeRainEffect::setBaseBrightness(uint8_t brightness) { _baseBrightness = brightness; }