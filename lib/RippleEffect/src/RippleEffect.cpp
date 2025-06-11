#include "RippleEffect.h"
#include <ArduinoJson.h> // Should be present for JSON parsing
#include "../../../include/DebugUtils.h" // Corrected path
#include "../../../include/TransitionUtils.h" // For DEFAULT_TRANSITION_DURATION_MS

// Updated presets to include the new 'sharpness' parameter
const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5,
    .speed = 4.0f,
    .thickness = 2.0f,
    .spawnIntervalS = 2.0f,
    .maxRadius = 16 * 1.2f,
    .randomOrigin = false,
    .saturation = 1.0f,
    .baseBrightness = 0.8f,
    .sharpness = 1.0f, // ADDED: A value of 1.0 gives the original soft cosine falloff
    .prePara = "WaterDrop"
};

const RippleEffect::Parameters RippleEffect::EnergyPulsePreset = {
    .maxRipples = 8,
    .speed = 8.0f,
    .thickness = 1.5f,
    .spawnIntervalS = 0.5f,
    .maxRadius = 16 * 1.5f,
    .randomOrigin = true,
    .saturation = 0.7f,
    .baseBrightness = 0.9f,
    .sharpness = 2.5f, // ADDED: A higher value for a sharper, more defined pulse
    .prePara = "EnergyPulse"
};

// Constructor and Destructor are unchanged
RippleEffect::RippleEffect() {
    _activeParams = WaterDropPreset;
    _targetParams = WaterDropPreset;
    _oldParams = WaterDropPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    _ripples = nullptr;
    _strip = nullptr;
    _matrixWidth = 0;
    _matrixHeight = 0;
    _nextRippleIndex = 0;
    _lastAutoRippleTimeMs = 0;
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

// setParameters(struct) is unchanged
void RippleEffect::setParameters(const Parameters& params) {
    DEBUG_PRINTLN("RippleEffect::setParameters(const Parameters&) called.");

    _oldParams = _activeParams;
    Parameters newTarget = params;

    // Handle maxRipples change immediately as it affects memory allocation.
    if (newTarget.maxRipples != _activeParams.maxRipples || _ripples == nullptr) {
        DEBUG_PRINTF("RippleEffect: maxRipples changing from %d to %d\n", _activeParams.maxRipples, newTarget.maxRipples);
        if (_ripples != nullptr) {
            delete[] _ripples;
            _ripples = nullptr;
        }
        // _targetParams must be set before new Ripple allocation if maxRipples comes from there.
        // However, newTarget already has the correct maxRipples.
        _ripples = new Ripple[newTarget.maxRipples];
        for (uint8_t i = 0; i < newTarget.maxRipples; ++i) {
            _ripples[i].isActive = false;
        }
        _nextRippleIndex = 0;

        // Reflect the change in maxRipples immediately in active and old params.
        _activeParams.maxRipples = newTarget.maxRipples;
        _oldParams.maxRipples = newTarget.maxRipples;
    }

    // Assign the full set of target parameters.
    _targetParams = newTarget;

    // Start the transition for other parameters.
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    DEBUG_PRINTLN("RippleEffect transition started.");
}

// Updated to parse the new 'sharpness' parameter from JSON
void RippleEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("RippleEffect::setParameters(json) called.");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("RippleEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc["maxRipples"].is<uint8_t>()) newParams.maxRipples = doc["maxRipples"].as<uint8_t>();
    if (doc["speed"].is<float>()) newParams.speed = doc["speed"].as<float>();
    if (doc["thickness"].is<float>()) newParams.thickness = doc["thickness"].as<float>();
    if (doc["spawnIntervalS"].is<float>()) newParams.spawnIntervalS = doc["spawnIntervalS"].as<float>();
    if (doc["maxRadius"].is<float>()) newParams.maxRadius = doc["maxRadius"].as<float>();
    if (doc["randomOrigin"].is<bool>()) newParams.randomOrigin = doc["randomOrigin"].as<bool>();
    if (doc["saturation"].is<float>()) newParams.saturation = doc["saturation"].as<float>();
    if (doc["baseBrightness"].is<float>()) newParams.baseBrightness = doc["baseBrightness"].as<float>();
    if (doc["sharpness"].is<float>()) newParams.sharpness = doc["sharpness"].as<float>();

    if (doc["prePara"].is<const char*>()) {
        const char* presetStr = doc["prePara"].as<const char*>();
        if (presetStr) {
            if (strcmp(presetStr, WaterDropPreset.prePara) == 0) newParams.prePara = WaterDropPreset.prePara;
            else if (strcmp(presetStr, EnergyPulsePreset.prePara) == 0) newParams.prePara = EnergyPulsePreset.prePara;
        }
    }

    setParameters(newParams);
}

// setPreset is unchanged
void RippleEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("RippleEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, "next") == 0) {
        const char* currentEffectivePreset = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;
        if (currentEffectivePreset == nullptr) currentEffectivePreset = WaterDropPreset.prePara;

        if (strcmp(currentEffectivePreset, WaterDropPreset.prePara) == 0) {
            setParameters(EnergyPulsePreset);
        } else {
            setParameters(WaterDropPreset);
        }
    } else if (strcmp(presetName, WaterDropPreset.prePara) == 0) {
        setParameters(WaterDropPreset);
    } else if (strcmp(presetName, EnergyPulsePreset.prePara) == 0) {
        setParameters(EnergyPulsePreset);
    } else {
        DEBUG_PRINTF("Unknown preset name in RippleEffect::setPreset: %s\n", presetName);
    }
}

// mapCoordinatesToIndex is unchanged
int RippleEffect::mapCoordinatesToIndex(int x, int y) {
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


void RippleEffect::Update() {
    if (_effectInTransition) {
        unsigned long currentTimeMs_lerp = millis(); // Renamed to avoid conflict
        unsigned long elapsedTime = currentTimeMs_lerp - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        // Interpolate parameters (maxRipples changes instantly and is handled in setParameters)
        _activeParams.speed = lerp(_oldParams.speed, _targetParams.speed, t);
        _activeParams.thickness = lerp(_oldParams.thickness, _targetParams.thickness, t);
        _activeParams.spawnIntervalS = lerp(_oldParams.spawnIntervalS, _targetParams.spawnIntervalS, t);
        _activeParams.maxRadius = lerp(_oldParams.maxRadius, _targetParams.maxRadius, t);
        _activeParams.randomOrigin = (t < 0.5f) ? _oldParams.randomOrigin : _targetParams.randomOrigin; // bool, switch halfway
        _activeParams.saturation = lerp(_oldParams.saturation, _targetParams.saturation, t);
        _activeParams.baseBrightness = lerp(_oldParams.baseBrightness, _targetParams.baseBrightness, t);
        _activeParams.sharpness = lerp(_oldParams.sharpness, _targetParams.sharpness, t);
        // prePara changes instantly with _targetParams

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams; // Ensure exact match at the end
            DEBUG_PRINTLN("RippleEffect transition complete.");
        }
    }

    if (_strip == nullptr || _ripples == nullptr || _matrixWidth == 0) return;

    unsigned long currentTimeMs = millis();

    // Auto-creation logic is unchanged
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_activeParams.spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;

        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;

        if (_activeParams.randomOrigin) {
            // Adjust random range for larger matrix
            originX += random(-_matrixWidth * 20, _matrixWidth * 20 + 1) / 100.0f;
            originY += random(-_matrixHeight * 20, _matrixHeight * 20 + 1) / 100.0f;
            originX = constrain(originX, 0.5f, _matrixWidth - 0.5f);
            originY = constrain(originY, 0.5f, _matrixHeight - 0.5f);
        }
        
        _ripples[_nextRippleIndex].isActive = true;
        _ripples[_nextRippleIndex].originX = originX;
        _ripples[_nextRippleIndex].originY = originY;
        _ripples[_nextRippleIndex].startTimeMs = currentTimeMs;
        _ripples[_nextRippleIndex].hue = random(0, 1000) / 1000.0f;
        _nextRippleIndex = (_nextRippleIndex + 1) % _activeParams.maxRipples;
    }

    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int y = 0; y < _matrixHeight; ++y) {
        for (int x = 0; x < _matrixWidth; ++x) {
            float pixelCenterX = x + 0.5f;
            float pixelCenterY = y + 0.5f;
            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0);

            for (int r_idx = 0; r_idx < _activeParams.maxRipples; ++r_idx) {
                if (_ripples[r_idx].isActive) {
                    float elapsedTimeS = (float)(currentTimeMs - _ripples[r_idx].startTimeMs) / 1000.0f;
                    float currentRadius = elapsedTimeS * _activeParams.speed - _activeParams.thickness / 2.0f;

                    if (currentRadius > _activeParams.maxRadius + _activeParams.thickness / 2.0f) {
                        _ripples[r_idx].isActive = false;
                        continue;
                    }

                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) + pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    float distToRippleEdge = abs(distToOrigin - currentRadius);

                    if (distToRippleEdge < _activeParams.thickness / 2.0f) {
                        
                        // --- CORE LOGIC CHANGE IS HERE ---
                        // 1. Calculate the base intensity using the cosine curve (from 0.0 to 1.0)
                        float baseIntensity = cos((distToRippleEdge / (_activeParams.thickness / 2.0f)) * (PI / 2.0f));
                        
                        // 2. Apply the 'sharpness' parameter using a power function
                        float intensityFactor = pow(baseIntensity, _activeParams.sharpness);
                        
                        // 3. The rest of the logic remains the same
                        intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);

                        if (intensityFactor > maxIntensityForThisPixel) {
                            maxIntensityForThisPixel = intensityFactor;
                            colorForThisPixel = HsbColor(_ripples[r_idx].hue, _activeParams.saturation, maxIntensityForThisPixel * _activeParams.baseBrightness);
                        }
                    }
                }
            }

            if (maxIntensityForThisPixel > 0.0f) {
                int led_index = mapCoordinatesToIndex(x, y);
                if (led_index >= 0 && led_index < _numLeds) {
                    _strip->SetPixelColor(led_index, colorForThisPixel);
                }
            }
        }
    }
}