#include "RippleEffect.h"
#include <ArduinoJson.h>
#include <math.h> // For sqrt, pow, fabs, cos. PI is in Arduino.h

// Preset definitions
const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5, .speed = 4.0f, .thickness = 2.0f, .spawnIntervalS = 2.0f,
    .maxRadius = 16 * 1.2f, .randomOrigin = false, .saturation = 1.0f,
    .baseBrightness = 0.8f, .sharpness = 1.0f,
    .shape = RippleEffect::Shape::CIRCLE,
    .shapeXScale = 1.0f,
    .shapeYScale = 1.0f,
    .prePara = "WaterDrop"
};

const RippleEffect::Parameters RippleEffect::EnergyPulsePreset = {
    .maxRipples = 8, .speed = 8.0f, .thickness = 1.5f, .spawnIntervalS = 0.5f,
    .maxRadius = 16 * 1.5f, .randomOrigin = true, .saturation = 0.7f,
    .baseBrightness = 0.9f, .sharpness = 2.5f,
    .shape = RippleEffect::Shape::CIRCLE,
    .shapeXScale = 1.0f,
    .shapeYScale = 1.0f,
    .prePara = "EnergyPulse"
};

const RippleEffect::Parameters RippleEffect::PointedHeartPreset = {
    .maxRipples = 5, .speed = 4.0f, .thickness = 2.0f, .spawnIntervalS = 2.0f, // Example base params
    .maxRadius = 16 * 1.2f, .randomOrigin = false, .saturation = 1.0f,
    .baseBrightness = 0.8f, .sharpness = 1.5f, // Unique sharpness for this preset
    .shape = RippleEffect::Shape::HEART,
    .shapeXScale = 1.0f,
    .shapeYScale = 1.5f, // For pointiness
    .prePara = "PointedHeart"
};

RippleEffect::RippleEffect() {
    _ripples = nullptr;
    _strip = nullptr;
    _matrixWidth = 0;
    _matrixHeight = 0;
    setParameters(WaterDropPreset); // Initialize with WaterDropPreset by default
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

void RippleEffect::setShape(Shape newShape) {
    _currentShape = newShape;
    _params.shape = newShape;
    // Apply default scales for the new shape directly to _params
    if (newShape == Shape::HEART) {
        _params.shapeXScale = PointedHeartPreset.shapeXScale;
        _params.shapeYScale = PointedHeartPreset.shapeYScale;
    } else if (newShape == Shape::CIRCLE) {
        _params.shapeXScale = 1.0f;
        _params.shapeYScale = 1.0f;
    }
}

void RippleEffect::setParameters(const Parameters& params) {
    bool oldRipplesNull = (_ripples == nullptr);
    uint8_t oldMaxRipples = _params.maxRipples;

    _params = params; // Full assignment, including new shape parameters
    _currentShape = _params.shape; // Update current shape from the newly assigned params

    bool needsReallocOrClear = false;
    if (oldMaxRipples != _params.maxRipples) {
        needsReallocOrClear = true;
    } else if (oldRipplesNull && _params.maxRipples > 0) {
        needsReallocOrClear = true;
    }

    if (needsReallocOrClear) {
        if (_ripples != nullptr) {
            delete[] _ripples;
            _ripples = nullptr;
        }
        if (_params.maxRipples > 0) {
            _ripples = new Ripple[_params.maxRipples];
            for (uint8_t i = 0; i < _params.maxRipples; ++i) {
                _ripples[i].isActive = false;
            }
        }
        _nextRippleIndex = 0;
    }
}

void RippleEffect::setParameters(const char* jsonParams) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.print(F("RippleEffect::setParameters failed to parse JSON: "));
        Serial.println(error.c_str());
        return;
    }

    Parameters newParams = _params; // Start with current parameters as a base

    // 1. Load preset if "prePara" is specified
    if (doc.containsKey("prePara")) {
        const char* presetNameStr = doc["prePara"].as<const char*>();
        if (presetNameStr) {
            if (strcmp(presetNameStr, "WaterDrop") == 0) newParams = WaterDropPreset;
            else if (strcmp(presetNameStr, "EnergyPulse") == 0) newParams = EnergyPulsePreset;
            else if (strcmp(presetNameStr, "PointedHeart") == 0) newParams = PointedHeartPreset;
        }
    }

    // 2. Override basic parameters from JSON
    if (doc.containsKey("maxRipples")) newParams.maxRipples = doc["maxRipples"].as<uint8_t>();
    newParams.speed = doc["speed"] | newParams.speed;
    newParams.thickness = doc["thickness"] | newParams.thickness;
    newParams.spawnIntervalS = doc["spawnIntervalS"] | newParams.spawnIntervalS;
    newParams.maxRadius = doc["maxRadius"] | newParams.maxRadius;
    if (doc.containsKey("randomOrigin")) newParams.randomOrigin = doc["randomOrigin"].as<bool>();
    newParams.saturation = doc["saturation"] | newParams.saturation;
    newParams.baseBrightness = doc["baseBrightness"] | newParams.baseBrightness;
    newParams.sharpness = doc["sharpness"] | newParams.sharpness;

    // 3. Override shape type if specified in JSON
    if (doc.containsKey("shape")) {
        const char* shapeStr = doc["shape"].as<const char*>();
        if (shapeStr) {
            if (strcmp(shapeStr, "heart") == 0) newParams.shape = Shape::HEART;
            else if (strcmp(shapeStr, "circle") == 0) newParams.shape = Shape::CIRCLE;
        }
    }
    // newParams.shape now holds the correct target shape (from JSON or preset)

    // 4. Set scales: if specified in JSON, use them. Otherwise, use defaults for the target shape in newParams.
    if (doc.containsKey("shape_x_scale")) {
        newParams.shapeXScale = doc["shape_x_scale"].as<float>();
    } else { // Not in JSON, ensure default for the current newParams.shape is applied
        if (newParams.shape == Shape::HEART) newParams.shapeXScale = PointedHeartPreset.shapeXScale;
        else newParams.shapeXScale = 1.0f; // Circle or other default
    }

    if (doc.containsKey("shape_y_scale")) {
        newParams.shapeYScale = doc["shape_y_scale"].as<float>();
    } else { // Not in JSON, ensure default for the current newParams.shape is applied
        if (newParams.shape == Shape::HEART) newParams.shapeYScale = PointedHeartPreset.shapeYScale;
        else newParams.shapeYScale = 1.0f; // Circle or other default
    }

    setParameters(newParams); // Call the struct version to apply all changes
}

void RippleEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "WaterDrop") == 0) {
        setParameters(WaterDropPreset);
    } else if (strcmp(presetName, "EnergyPulse") == 0) {
        setParameters(EnergyPulsePreset);
    } else if (strcmp(presetName, "PointedHeart") == 0) {
        setParameters(PointedHeartPreset);
    } else if (strcmp(presetName, "next") == 0) {
        if (_params.prePara && strcmp(_params.prePara, "WaterDrop") == 0) {
            setParameters(EnergyPulsePreset);
        } else if (_params.prePara && strcmp(_params.prePara, "EnergyPulse") == 0) {
            setParameters(PointedHeartPreset);
        } else { // If PointedHeart or undefined, cycle to WaterDrop
            setParameters(WaterDropPreset);
        }
    } else {
        Serial.print(F("RippleEffect::setPreset - Unknown preset: "));
        Serial.println(presetName);
    }
}

// mapCoordinatesToIndex - assuming this is the correct version from before
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
    if (final_index >= 0 && final_index < _numLeds) { return final_index; }
    return -1;
}

void RippleEffect::Update() {
    if (_strip == nullptr || _matrixWidth == 0) return;
    if (_params.maxRipples == 0 || _ripples == nullptr) {
        _strip->ClearTo(RgbColor(0,0,0));
        return;
    }

    unsigned long currentTimeMs = millis();

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
    for (int y_coord = 0; y_coord < _matrixHeight; ++y_coord) {
        for (int x_coord = 0; x_coord < _matrixWidth; ++x_coord) {
            float pixelCenterX = x_coord + 0.5f;
            float pixelCenterY = y_coord + 0.5f;
            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0);

            for (int r_idx = 0; r_idx < _params.maxRipples; ++r_idx) {
                if (!_ripples[r_idx].isActive) continue;

                float elapsedTimeS = (float)(currentTimeMs - _ripples[r_idx].startTimeMs) / 1000.0f;
                float expansionFactor = elapsedTimeS * _params.speed; // General size/progression of ripple

                // Deactivation logic (consistent with previous version)
                float effectiveRadiusForDeactivation = expansionFactor - _params.thickness / 2.0f;
                if (effectiveRadiusForDeactivation > _params.maxRadius + _params.thickness / 2.0f) {
                    _ripples[r_idx].isActive = false;
                    continue;
                }
                if (effectiveRadiusForDeactivation + _params.thickness < 0) { // Ripple band not yet visible
                    continue;
                }

                float distToRippleEdge = _params.thickness; // Default to outside band

                if (_currentShape == Shape::CIRCLE) {
                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) +
                                              pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    distToRippleEdge = fabs(distToOrigin - expansionFactor);
                } else if (_currentShape == Shape::HEART) {
                    if (expansionFactor < 0.01f) { // Avoid issues if heart is tiny
                        distToRippleEdge = _params.thickness;
                    } else {
                        float dx = pixelCenterX - _ripples[r_idx].originX;
                        float dy = -(pixelCenterY - _ripples[r_idx].originY); // Invert for conventional heart

                        // Apply scaling factors
                        float normX = dx / (_params.shapeXScale * expansionFactor);
                        float normY = dy / (_params.shapeYScale * expansionFactor);

                        float term_sq_sum = normX * normX + normY * normY;
                        float term_minus_1 = term_sq_sum - 1.0f;
                        float heartValue = pow(term_minus_1, 3.0f) - (normX * normX * pow(normY, 3.0f));

                        const float heartEqThreshold = 0.03f; // Tunable
                        distToRippleEdge = (fabs(heartValue) / heartEqThreshold) * (_params.thickness / 2.0f);
                    }
                }

                // Common intensity calculation
                if (distToRippleEdge < _params.thickness / 2.0f) {
                    float baseIntensity = cos((distToRippleEdge / (_params.thickness / 2.0f)) * (PI / 2.0f));
                    float intensityFactor = pow(baseIntensity, _params.sharpness);
                    intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);

                    if (intensityFactor > maxIntensityForThisPixel) {
                        maxIntensityForThisPixel = intensityFactor;
                        colorForThisPixel = HsbColor(_ripples[r_idx].hue, _params.saturation, maxIntensityForThisPixel * _params.baseBrightness);
                    }
                }
            }

            if (maxIntensityForThisPixel > 0.0f) {
                int led_index = mapCoordinatesToIndex(x_coord, y_coord);
                if (led_index >= 0 && led_index < _numLeds) {
                    _strip->SetPixelColor(led_index, colorForThisPixel);
                }
            }
        }
    }
}