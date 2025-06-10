#include "RippleEffect.h"
#include <ArduinoJson.h>
#include <math.h> // For sqrt, pow, abs, cos. PI is in Arduino.h

// Preset definitions (without .shape)
const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5,
    .speed = 4.0f,
    .thickness = 2.0f,
    .spawnIntervalS = 2.0f,
    .maxRadius = 16 * 1.2f,
    .randomOrigin = false,
    .saturation = 1.0f,
    .baseBrightness = 0.8f,
    .sharpness = 1.0f, // Sharpness was present
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
    .sharpness = 2.5f, // Sharpness was present
    .prePara = "EnergyPulse"
};

RippleEffect::RippleEffect() {
    _ripples = nullptr;
    _strip = nullptr;
    _matrixWidth = 0;
    _matrixHeight = 0;
    setParameters(WaterDropPreset); // Initialize with a default preset
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

// Reverted setParameters(const Parameters& params)
void RippleEffect::setParameters(const Parameters& params) {
    bool numRipplesChanged = (_params.maxRipples != params.maxRipples);
    _params = params; // Assign new parameters

    if (numRipplesChanged || _ripples == nullptr) {
        if (_ripples != nullptr) {
            delete[] _ripples;
            _ripples = nullptr; // Good practice to nullify after delete
        }
        // Only allocate if maxRipples is positive
        if (_params.maxRipples > 0) {
            _ripples = new Ripple[_params.maxRipples];
            for (uint8_t i = 0; i < _params.maxRipples; ++i) {
                _ripples[i].isActive = false;
            }
        }
        _nextRippleIndex = 0;
    }
}

// Reverted setParameters(const char* jsonParams)
void RippleEffect::setParameters(const char* jsonParams) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.print(F("RippleEffect::setParameters failed to parse JSON: "));
        Serial.println(error.c_str());
        return;
    }

    Parameters tempParams = _params; // Start with a copy of current params

    // Check if a preset is being applied first
    if (doc.containsKey("prePara")) {
        const char* presetNameStr = doc["prePara"].as<const char*>();
        if (presetNameStr) {
            if (strcmp(presetNameStr, "WaterDrop") == 0) {
                tempParams = WaterDropPreset;
            } else if (strcmp(presetNameStr, "EnergyPulse") == 0) {
                tempParams = EnergyPulsePreset;
            }
            // If other known presets, add them here.
            // Note: tempParams.prePara will be updated by assigning the whole preset struct.
        }
    }

    // Override with specific values from JSON, if present
    // This allows individual params to override those from a loaded preset in the same JSON command
    if (doc.containsKey("maxRipples")) tempParams.maxRipples = doc["maxRipples"].as<uint8_t>();
    if (doc.containsKey("speed")) tempParams.speed = doc["speed"].as<float>();
    if (doc.containsKey("thickness")) tempParams.thickness = doc["thickness"].as<float>();
    if (doc.containsKey("spawnIntervalS")) tempParams.spawnIntervalS = doc["spawnIntervalS"].as<float>();
    if (doc.containsKey("maxRadius")) tempParams.maxRadius = doc["maxRadius"].as<float>();
    if (doc.containsKey("randomOrigin")) tempParams.randomOrigin = doc["randomOrigin"].as<bool>();
    if (doc.containsKey("saturation")) tempParams.saturation = doc["saturation"].as<float>();
    if (doc.containsKey("baseBrightness")) tempParams.baseBrightness = doc["baseBrightness"].as<float>();
    if (doc.containsKey("sharpness")) tempParams.sharpness = doc["sharpness"].as<float>(); // Sharpness was present

    setParameters(tempParams); // Call the struct version to apply and handle ripple array
}

void RippleEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "WaterDrop") == 0) {
        setParameters(WaterDropPreset);
    } else if (strcmp(presetName, "EnergyPulse") == 0) {
        setParameters(EnergyPulsePreset);
    } else if (strcmp(presetName, "next") == 0) {
        if (_params.prePara && strcmp(_params.prePara, "WaterDrop") == 0) { // Check current preset name
            setParameters(EnergyPulsePreset);
        } else {
            setParameters(WaterDropPreset); // Default or if current is EnergyPulse
        }
    } else {
        Serial.print(F("RippleEffect::setPreset - Unknown preset: "));
        Serial.println(presetName);
    }
}

// mapCoordinatesToIndex was not changed by heart shape feature, keep as is.
// Assuming the version from the previous read_files is the correct one.
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

    int final_index = base_index + local_offset;
    if (final_index >= 0 && final_index < _numLeds) { // Check bounds
        return final_index;
    }
    return -1; // Invalid coordinate
}

// Reverted Update() method
void RippleEffect::Update() {
    if (_strip == nullptr || _matrixWidth == 0) return;
    if (_params.maxRipples == 0 || _ripples == nullptr) { // If no ripples configured
        _strip->ClearTo(RgbColor(0,0,0)); // Clear strip and return
        return;
    }

    unsigned long currentTimeMs = millis();

    // Auto-creation logic (original, assuming sharpness was added to this base)
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_params.spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;

        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;

        if (_params.randomOrigin) {
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
        _nextRippleIndex = (_nextRippleIndex + 1) % _params.maxRipples;
    }

    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int y = 0; y < _matrixHeight; ++y) {
        for (int x = 0; x < _matrixWidth; ++x) {
            float pixelCenterX = x + 0.5f;
            float pixelCenterY = y + 0.5f;
            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0);

            for (int r_idx = 0; r_idx < _params.maxRipples; ++r_idx) {
                if (_ripples[r_idx].isActive) {
                    float elapsedTimeS = (float)(currentTimeMs - _ripples[r_idx].startTimeMs) / 1000.0f;
                    // This is the original currentRadius definition (center of the band)
                    float currentRadius = elapsedTimeS * _params.speed;

                    // Original deactivation: ripple "disappears" when its *center* + thickness/2 goes beyond maxRadius
                    // Or more simply, when its center is too far.
                    // The version with sharpness had:
                    // float currentRadius = elapsedTimeS * _params.speed - _params.thickness / 2.0f;
                    // if (currentRadius > _params.maxRadius + _params.thickness / 2.0f) { ... }
                    // Let's use that "effective radius" logic for consistency with sharpness version.
                    float effectiveCurrentRadius = elapsedTimeS * _params.speed - _params.thickness / 2.0f;

                    if (effectiveCurrentRadius > _params.maxRadius + _params.thickness / 2.0f) {
                         _ripples[r_idx].isActive = false;
                         continue;
                    }
                    // Also handle case where ripple hasn't "started" yet.
                    if (effectiveCurrentRadius + _params.thickness < 0) { // If the entire band is before 0
                        continue;
                    }


                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) + pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    // distToRippleEdge is distance from pixel to the *center line* of the ripple band
                    float distToRippleEdge = fabs(distToOrigin - (elapsedTimeS * _params.speed) );


                    if (distToRippleEdge < _params.thickness / 2.0f) {
                        float baseIntensity = cos((distToRippleEdge / (_params.thickness / 2.0f)) * (PI / 2.0f)); // PI from Arduino.h
                        float intensityFactor = pow(baseIntensity, _params.sharpness); // Sharpness feature
                        intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);

                        if (intensityFactor > maxIntensityForThisPixel) {
                            maxIntensityForThisPixel = intensityFactor;
                            colorForThisPixel = HsbColor(_ripples[r_idx].hue, _params.saturation, maxIntensityForThisPixel * _params.baseBrightness);
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