#include "CodeRainEffect.h"
#include <ArduinoJson.h>
#include <cstring> // For strcmp
#include "../../../include/DebugUtils.h" // For DEBUG_PRINTLN, DEBUG_PRINTF
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
    _activeParams = ClassicMatrixPreset;
    _targetParams = ClassicMatrixPreset;
    _oldParams = ClassicMatrixPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    _codeStreams = nullptr;
    _strip = nullptr;
}

CodeRainEffect::~CodeRainEffect() {
    if (_codeStreams != nullptr) {
        delete[] _codeStreams;
    }
}

void CodeRainEffect::setParameters(const Parameters& params) {
    DEBUG_PRINTLN("CodeRainEffect::setParameters(const Parameters&) called.");
    if (_effectInTransition) {
        // If already transitioning, current _activeParams is the most up-to-date "old" state
        _oldParams = _activeParams;
    } else {
        // If not in transition, current _activeParams is the true old state
        _oldParams = _activeParams;
    }
    _targetParams = params;
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    // Use the default transition duration, or a specific one if params include it
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    // Logic for _codeStreams initialization or reset, using _targetParams
    // This part is important if parameters affecting stream structure/count change.
    // For CodeRain, stream count is fixed to _matrixWidth.
    // Resetting cooldowns might be desirable when parameters change.
    if (_codeStreams == nullptr && _matrixWidth > 0) {
        _codeStreams = new CodeStream[_matrixWidth];
        for (uint8_t i = 0; i < _matrixWidth; ++i) {
            _codeStreams[i].isActive = false;
            // Initialize cooldowns to allow immediate spawning if probability allows
            _codeStreams[i].lastActivityTimeMs = millis() - random(_targetParams.minSpawnCooldownMs, _targetParams.maxSpawnCooldownMs);
            _codeStreams[i].spawnCooldownMs = random(_targetParams.minSpawnCooldownMs, _targetParams.maxSpawnCooldownMs);
        }
    } else if (_codeStreams != nullptr) {
        // If parameters that affect stream behavior are changed, reset relevant stream properties
        for (uint8_t i = 0; i < _matrixWidth; ++i) {
            // Example: Reset spawn cooldown based on new target parameters
            _codeStreams[i].spawnCooldownMs = random(_targetParams.minSpawnCooldownMs, _targetParams.maxSpawnCooldownMs);
            // Potentially reset lastActivityTimeMs to force re-evaluation against new cooldown
            _codeStreams[i].lastActivityTimeMs = millis() - random(0, _codeStreams[i].spawnCooldownMs);
        }
    }
    DEBUG_PRINTLN("CodeRainEffect transition started.");
}

void CodeRainEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("CodeRainEffect::setParameters(json) called [RE_APPLIED_FIX].");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("CodeRainEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc["minSpeed"].is<float>()) newParams.minSpeed = doc["minSpeed"].as<float>();
    if (doc["maxSpeed"].is<float>()) newParams.maxSpeed = doc["maxSpeed"].as<float>();
    if (doc["minStreamLength"].is<int>()) newParams.minStreamLength = doc["minStreamLength"].as<int>();
    if (doc["maxStreamLength"].is<int>()) newParams.maxStreamLength = doc["maxStreamLength"].as<int>();
    if (doc["spawnProbability"].is<float>()) newParams.spawnProbability = doc["spawnProbability"].as<float>();
    if (doc["minSpawnCooldownMs"].is<unsigned long>()) newParams.minSpawnCooldownMs = doc["minSpawnCooldownMs"].as<unsigned long>();
    if (doc["maxSpawnCooldownMs"].is<unsigned long>()) newParams.maxSpawnCooldownMs = doc["maxSpawnCooldownMs"].as<unsigned long>();
    if (doc["baseHue"].is<float>()) newParams.baseHue = doc["baseHue"].as<float>();
    if (doc["hueVariation"].is<float>()) newParams.hueVariation = doc["hueVariation"].as<float>();
    if (doc["saturation"].is<float>()) newParams.saturation = doc["saturation"].as<float>();
    if (doc["baseBrightness"].is<float>()) newParams.baseBrightness = doc["baseBrightness"].as<float>();
    
    if (doc["prePara"].is<const char*>()) {
        const char* presetStr = doc["prePara"].as<const char*>();
        if (presetStr) {
            if (strcmp(presetStr, ClassicMatrixPreset.prePara) == 0) {
                newParams.prePara = ClassicMatrixPreset.prePara;
            } else if (strcmp(presetStr, FastGlitchPreset.prePara) == 0) {
                newParams.prePara = FastGlitchPreset.prePara;
            }
        }
    }

    setParameters(newParams); // Call the struct version to handle logic and transition
}

void CodeRainEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("CodeRainEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, "next") == 0) {
        // Determine the current preset based on target if transitioning, otherwise active
        const char* currentEffectivePresetName = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;

        // Fallback if prePara is somehow null or not set
        if (currentEffectivePresetName == nullptr) {
            currentEffectivePresetName = ClassicMatrixPreset.prePara;
        }

        if (strcmp(currentEffectivePresetName, ClassicMatrixPreset.prePara) == 0) {
            setParameters(FastGlitchPreset); // This will trigger a transition
            DEBUG_PRINTLN("Switching to FastGlitchPreset via 'next'");
        } else {
            setParameters(ClassicMatrixPreset); // This will trigger a transition
            DEBUG_PRINTLN("Switching to ClassicMatrixPreset via 'next'");
        }
    } else if (strcmp(presetName, ClassicMatrixPreset.prePara) == 0) {
        setParameters(ClassicMatrixPreset); // Triggers transition
        DEBUG_PRINTLN("Setting ClassicMatrixPreset");
    } else if (strcmp(presetName, FastGlitchPreset.prePara) == 0) {
        setParameters(FastGlitchPreset); // Triggers transition
        DEBUG_PRINTLN("Setting FastGlitchPreset");
    } else {
        DEBUG_PRINTF("Unknown preset name in CodeRainEffect::setPreset: %s\n", presetName);
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
    if (_effectInTransition) {
        unsigned long currentTimeMs = millis();
        unsigned long elapsedTime = currentTimeMs - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        // Interpolate parameters
        _activeParams.minSpeed = lerp(_oldParams.minSpeed, _targetParams.minSpeed, t);
        _activeParams.maxSpeed = lerp(_oldParams.maxSpeed, _targetParams.maxSpeed, t);
        _activeParams.minStreamLength = lerp(_oldParams.minStreamLength, _targetParams.minStreamLength, t);
        _activeParams.maxStreamLength = lerp(_oldParams.maxStreamLength, _targetParams.maxStreamLength, t);
        _activeParams.spawnProbability = lerp(_oldParams.spawnProbability, _targetParams.spawnProbability, t);
        // For unsigned long, ensure lerp result is cast back if necessary and fits
        _activeParams.minSpawnCooldownMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.minSpawnCooldownMs), static_cast<int>(_targetParams.minSpawnCooldownMs), t));
        _activeParams.maxSpawnCooldownMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.maxSpawnCooldownMs), static_cast<int>(_targetParams.maxSpawnCooldownMs), t));
        _activeParams.baseHue = lerp(_oldParams.baseHue, _targetParams.baseHue, t);
        _activeParams.hueVariation = lerp(_oldParams.hueVariation, _targetParams.hueVariation, t);
        _activeParams.saturation = lerp(_oldParams.saturation, _targetParams.saturation, t);
        _activeParams.baseBrightness = lerp(_oldParams.baseBrightness, _targetParams.baseBrightness, t);
        // _activeParams.prePara is not interpolated; it changes instantly when _targetParams is set,
        // or more accurately, _activeParams.prePara will reflect _targetParams.prePara once transition ends.

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams; // Ensure exact match at the end
            DEBUG_PRINTLN("CodeRainEffect transition complete.");
        }
    }

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
                _codeStreams[x].spawnCooldownMs = random(_activeParams.minSpawnCooldownMs, _activeParams.maxSpawnCooldownMs);
            }
        } else {
            if (currentTimeMs - _codeStreams[x].lastActivityTimeMs >= _codeStreams[x].spawnCooldownMs) {
                if (random(1000) / 1000.0f < _activeParams.spawnProbability) {
                    _codeStreams[x].isActive = true;
                    _codeStreams[x].currentY = -random(0, _matrixHeight * 2);
                    _codeStreams[x].speed = random(_activeParams.minSpeed * 100, _activeParams.maxSpeed * 100) / 100.0f;
                    _codeStreams[x].length = random(_activeParams.minStreamLength, _activeParams.maxStreamLength + 1);
                    _codeStreams[x].hue = _activeParams.baseHue + (random(-_activeParams.hueVariation * 100, _activeParams.hueVariation * 100) / 100.0f);
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
                    
                    HsbColor hsbColor(_codeStreams[x].hue, _activeParams.saturation, brightnessFactor * _activeParams.baseBrightness);
                    int ledIndex = mapCoordinatesToIndex(x, charY);

                    if (ledIndex >= 0 && ledIndex < _numLeds) {
                        _strip->SetPixelColor(ledIndex, hsbColor);
                    }
                }
            }
        }
    }
}
