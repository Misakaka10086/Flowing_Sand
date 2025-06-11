#include "RippleEffect.h"
#include <ArduinoJson.h>

// Updated presets to include the new 'sharpness' parameter
const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5,
    .speed = 4.0f,
    .acceleration = 0.0f,
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
    .acceleration = 0.0f,
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
    _ripples = nullptr;
    _strip = nullptr;
    _matrixWidth = 0;
    _matrixHeight = 0;
    setParameters(WaterDropPreset);
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

// setParameters(struct) is unchanged
void RippleEffect::setParameters(const Parameters& params) {
    bool numRipplesChanged = (_params.maxRipples != params.maxRipples);
    _params = params;

    if (numRipplesChanged || _ripples == nullptr) {
        if (_ripples != nullptr) delete[] _ripples;
        _ripples = new Ripple[_params.maxRipples];
        for (uint8_t i = 0; i < _params.maxRipples; ++i) {
            _ripples[i].isActive = false;
        }
        _nextRippleIndex = 0;
    }
}

// Updated to parse the new 'sharpness' parameter from JSON
void RippleEffect::setParameters(const char* jsonParams) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.println("RippleEffect::setParameters failed to parse JSON: " + String(error.c_str()));
        return;
    }
    if (doc["maxRipples"].is<uint8_t>()) {
        uint8_t newMaxRipples = doc["maxRipples"].as<uint8_t>();
        if (newMaxRipples != _params.maxRipples) {
            _params.maxRipples = newMaxRipples;
            setParameters(_params);
        }
    }
    _params.speed = doc["speed"] | _params.speed;
    _params.acceleration = doc["acceleration"] | _params.acceleration;
    _params.thickness = doc["thickness"] | _params.thickness;
    _params.spawnIntervalS = doc["spawnIntervalS"] | _params.spawnIntervalS;
    _params.maxRadius = doc["maxRadius"] | _params.maxRadius;
    _params.randomOrigin = doc["randomOrigin"] | _params.randomOrigin;
    _params.saturation = doc["saturation"] | _params.saturation;
    _params.baseBrightness = doc["baseBrightness"] | _params.baseBrightness;
    _params.sharpness = doc["sharpness"] | _params.sharpness; // ADDED: Parse sharpness

    // Preset logic remains unchanged
    if (doc["prePara"].is<String>()) {
        const char* newPrePara = doc["prePara"].as<String>().c_str();
        if (strcmp(newPrePara, "WaterDrop") == 0) _params.prePara = WaterDropPreset.prePara;
        else if (strcmp(newPrePara, "EnergyPulse") == 0) _params.prePara = EnergyPulsePreset.prePara;
    }
}

// setPreset is unchanged
void RippleEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "next") == 0) {
        if (strcmp(_params.prePara, "WaterDrop") == 0) setParameters(EnergyPulsePreset);
        else setParameters(WaterDropPreset);
    } else if (strcmp(presetName, "WaterDrop") == 0) {
        setParameters(WaterDropPreset);
    } else if (strcmp(presetName, "EnergyPulse") == 0) {
        setParameters(EnergyPulsePreset);
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
    if (_strip == nullptr || _ripples == nullptr || _matrixWidth == 0) return;

    unsigned long currentTimeMs = millis();

    // Auto-creation logic is unchanged
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_params.spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;

        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;

        if (_params.randomOrigin) {
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
                    float currentRadius = elapsedTimeS * _params.speed - _params.thickness / 2.0f;
                    currentRadius += _params.acceleration * elapsedTimeS;
                    if (currentRadius > _params.maxRadius + _params.thickness / 2.0f) {
                        _ripples[r_idx].isActive = false;
                        continue;
                    }

                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) + pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    float distToRippleEdge = abs(distToOrigin - currentRadius);

                    if (distToRippleEdge < _params.thickness / 2.0f) {
                        
                        // --- CORE LOGIC CHANGE IS HERE ---
                        // 1. Calculate the base intensity using the cosine curve (from 0.0 to 1.0)
                        float baseIntensity = cos((distToRippleEdge / (_params.thickness / 2.0f)) * (PI / 2.0f));
                        
                        // 2. Apply the 'sharpness' parameter using a power function
                        float intensityFactor = pow(baseIntensity, _params.sharpness);
                        
                        // 3. The rest of the logic remains the same
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