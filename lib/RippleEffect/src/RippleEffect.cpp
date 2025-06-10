#include "RippleEffect.h"
#include <ArduinoJson.h>
#include <cmath> // For fabsf, powf, sqrtf, cosf

// Ensure M_PI is available (usually from cmath or math.h)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const RippleEffect::Parameters RippleEffect::WaterDropPreset = {
    .maxRipples = 5, .speed = 4.0f, .thickness = 2.0f, .spawnIntervalS = 2.0f,
    .maxRadius = 16 * 1.2f, .randomOrigin = false, .saturation = 1.0f,
    .baseBrightness = 0.8f, .sharpness = 1.0f,
    .shape = RippleEffect::Shape::CIRCLE, // Added
    .prePara = "WaterDrop"
};

const RippleEffect::Parameters RippleEffect::EnergyPulsePreset = {
    .maxRipples = 8, .speed = 8.0f, .thickness = 1.5f, .spawnIntervalS = 0.5f,
    .maxRadius = 16 * 1.5f, .randomOrigin = true, .saturation = 0.7f,
    .baseBrightness = 0.9f, .sharpness = 2.5f,
    .shape = RippleEffect::Shape::CIRCLE, // Added
    .prePara = "EnergyPulse"
};


RippleEffect::RippleEffect() {
    _ripples = nullptr;
    _strip = nullptr;
    _matrixWidth = 0;
    _matrixHeight = 0;
    // Initialize _params with a default preset, which also sets _currentShape via setParameters.
    setParameters(WaterDropPreset);
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

void RippleEffect::setShape(Shape newShape) {
    _currentShape = newShape;
    _params.shape = newShape; // Keep _params consistent for when setParameters is called or params are fetched
}

void RippleEffect::setParameters(const Parameters& params) {
    bool oldRipplesNull = (_ripples == nullptr);
    uint8_t oldMaxRipples = _params.maxRipples; // Store old value before _params is overwritten

    _params = params; // Assign new parameters
    _currentShape = _params.shape; // Update current shape from newly assigned params

    bool needsReallocOrClear = false;
    // Check if reallocation is needed
    if (oldMaxRipples != _params.maxRipples) { // If maxRipples count changed
        needsReallocOrClear = true;
    } else if (oldRipplesNull && _params.maxRipples > 0) { // Was null, now needs allocation
        needsReallocOrClear = true;
    }
    // This also covers the case where oldMaxRipples > 0 and _params.maxRipples becomes 0.

    if (needsReallocOrClear) {
        if (_ripples != nullptr) {
            delete[] _ripples;
            _ripples = nullptr;
        }
        if (_params.maxRipples > 0) { // Only allocate if new count is positive
            _ripples = new Ripple[_params.maxRipples];
            for (uint8_t i = 0; i < _params.maxRipples; ++i) {
                _ripples[i].isActive = false; // Initialize new ripples
            }
        }
        _nextRippleIndex = 0; // Reset index after reallocation
    }
    // If not reallocated but ripples exist, ensure they are reset (optional, could also let them continue)
    // For simplicity, we don't reset them here if maxRipples didn't change.
    // If _params.maxRipples is 0, _ripples will be nullptr or become nullptr above.
}

void RippleEffect::setParameters(const char* jsonParams) {
    JsonDocument doc; // For ArduinoJson 6.x
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        Serial.print(F("RippleEffect::setParameters failed to parse JSON: ")); // Use F() for string literals
        Serial.println(error.c_str());
        return;
    }

    Parameters newParams = _params; // Start with current parameters as a base

    // If "prePara" (preset name) is in JSON, load that preset first.
    // This preset becomes the new base, overridable by other JSON fields.
    if (doc.containsKey("prePara")) {
        const char* presetNameStr = doc["prePara"].as<const char*>();
        if (presetNameStr) { // Check if string conversion was successful
            if (strcmp(presetNameStr, "WaterDrop") == 0) {
                newParams = WaterDropPreset;
            } else if (strcmp(presetNameStr, "EnergyPulse") == 0) {
                newParams = EnergyPulsePreset;
            }
        }
    }

    // Override with specific values from JSON, if present
    if (doc.containsKey("maxRipples")) newParams.maxRipples = doc["maxRipples"].as<uint8_t>();

    // Use pipe operator for default values: if key not present or type is wrong, newParams' current value from base (preset or _params) is used.
    newParams.speed = doc["speed"] | newParams.speed;
    newParams.thickness = doc["thickness"] | newParams.thickness;
    newParams.spawnIntervalS = doc["spawnIntervalS"] | newParams.spawnIntervalS;
    newParams.maxRadius = doc["maxRadius"] | newParams.maxRadius;

    if (doc.containsKey("randomOrigin")) newParams.randomOrigin = doc["randomOrigin"].as<bool>(); // Explicit for bool
    // else newParams.randomOrigin remains as per preset or current _params

    newParams.saturation = doc["saturation"] | newParams.saturation;
    newParams.baseBrightness = doc["baseBrightness"] | newParams.baseBrightness;
    newParams.sharpness = doc["sharpness"] | newParams.sharpness;

    if (doc.containsKey("shape")) {
        const char* shapeStr = doc["shape"].as<const char*>();
        if (shapeStr) { // Check if string conversion was successful
            if (strcmp(shapeStr, "heart") == 0) {
                newParams.shape = Shape::HEART;
            } else if (strcmp(shapeStr, "circle") == 0) {
                newParams.shape = Shape::CIRCLE;
            }
            // Add other shapes here if needed
        }
    }

    // Call the struct version of setParameters to apply changes
    // and handle ripple array reallocation if needed
    setParameters(newParams);
}


void RippleEffect::setPreset(const char* presetName) {
    if (strcmp(presetName, "WaterDrop") == 0) {
        setParameters(WaterDropPreset);
    } else if (strcmp(presetName, "EnergyPulse") == 0) {
        setParameters(EnergyPulsePreset);
    } else if (strcmp(presetName, "next") == 0) { // Added "next" preset logic for cycling
        // This assumes prePara in _params is correctly reflecting the current preset
        if (_params.prePara && strcmp(_params.prePara, "WaterDrop") == 0) {
            setParameters(EnergyPulsePreset);
        } else {
            setParameters(WaterDropPreset); // Default to WaterDrop if current is not WaterDrop (e.g. EnergyPulse or undefined)
        }
    } else {
        Serial.print(F("RippleEffect::setPreset - Unknown preset: "));
        Serial.println(presetName);
    }
}


void RippleEffect::Update() {
    // Check for valid state; especially if maxRipples is 0, _ripples will be nullptr.
    if (_strip == nullptr || _matrixWidth == 0 ) { // Basic checks
        return;
    }
    // If maxRipples is 0, _ripples should be nullptr (handled by setParameters).
    // If _ripples is nullptr (e.g. maxRipples became 0), clear strip and return.
    if (_params.maxRipples == 0 || _ripples == nullptr) {
        if (_strip) _strip->ClearTo(RgbColor(0,0,0));
        return;
    }


    unsigned long currentTimeMs = millis();

    // Auto-spawn new ripples
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_params.spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;
        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;
        if (_params.randomOrigin) {
            // Using integer math for random part to avoid float issues with random() range
            // Scaled by 100 to get two decimal places of precision for origin offset
            originX += random(-_matrixWidth * 20, _matrixWidth * 20 + 1) / 100.0f;
            originY += random(-_matrixHeight * 20, _matrixHeight * 20 + 1) / 100.0f;
            originX = constrain(originX, 0.5f, _matrixWidth - 0.5f);
            originY = constrain(originY, 0.5f, _matrixHeight - 0.5f);
        }
        _ripples[_nextRippleIndex].isActive = true;
        _ripples[_nextRippleIndex].originX = originX;
        _ripples[_nextRippleIndex].originY = originY;
        _ripples[_nextRippleIndex].startTimeMs = currentTimeMs;
        _ripples[_nextRippleIndex].hue = (random(0, 1000) / 1000.0f); // random() returns long
        _nextRippleIndex = (_nextRippleIndex + 1) % _params.maxRipples;
    }

    _strip->ClearTo(RgbColor(0, 0, 0)); // Clear strip at the beginning of each frame
    for (int y_coord = 0; y_coord < _matrixHeight; ++y_coord) { // Renamed y to y_coord
        for (int x_coord = 0; x_coord < _matrixWidth; ++x_coord) { // Renamed x to x_coord
            float pixelCenterX = x_coord + 0.5f;
            float pixelCenterY = y_coord + 0.5f;
            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0); // Default to black

            for (int r_idx = 0; r_idx < _params.maxRipples; ++r_idx) {
                if (!_ripples[r_idx].isActive) continue;

                float elapsedTimeS = (float)(currentTimeMs - _ripples[r_idx].startTimeMs) / 1000.0f;
                // currentRadius is the center of the ripple band
                float currentRadius = elapsedTimeS * _params.speed;

                // Effective radius for deactivation check should consider the thickness
                // Ripple is "over" when its leading edge (center + thickness/2) exceeds maxRadius
                // Or when its trailing edge (center - thickness/2) is still negative (not yet started visibly)
                // For simplicity in deactivation: check if center has gone way past maxRadius
                if (currentRadius - _params.thickness / 2.0f > _params.maxRadius) {
                    _ripples[r_idx].isActive = false;
                    continue;
                }
                 // If currentRadius is negative, it means it hasn't "started" yet from center.
                 // For a ripple band, we're interested if the band is visible.
                 // If currentRadius + _params.thickness / 2.0f < 0, the entire band is "before" the origin time.
                if (currentRadius + _params.thickness / 2.0f < 0) {
                    // This condition might not be strictly necessary if startTime is always currentTimeMs
                    // but good for robustness if ripples could be initialized with past/future start times.
                    continue;
                }


                float distToRippleEdge = _params.thickness; // Default to a value that results in zero intensity

                if (_currentShape == Shape::CIRCLE) {
                    float distToOrigin = sqrtf(powf(pixelCenterX - _ripples[r_idx].originX, 2.0f) +
                                               powf(pixelCenterY - _ripples[r_idx].originY, 2.0f));
                    // distToRippleEdge is the distance from the pixel to the center of the ripple band
                    distToRippleEdge = fabsf(distToOrigin - currentRadius);
                } else if (_currentShape == Shape::HEART) {
                    float hx = pixelCenterX - _ripples[r_idx].originX;
                    // Invert hy because matrix Y is downwards, but heart equation Y is typically upwards
                    float hy = -(pixelCenterY - _ripples[r_idx].originY);

                    // Avoid division by zero or very small scaleFactor for heart
                    // currentRadius here acts as the "size" of the heart.
                    if (currentRadius < 0.01f) { // If heart is too small, consider pixel outside
                        distToRippleEdge = _params.thickness;
                    } else {
                        // Normalize coordinates by currentRadius to make the heart equation scale with radius
                        // A larger radius means a larger heart.
                        // The aspect ratio of the heart can be tuned by pre-scaling hx or hy
                        // For example, if matrix is wide: hy = hy * (_matrixWidth / _matrixHeight)
                        // For now, assume 1:1 pixel aspect for heart scaling.
                        float scaled_hx = hx / currentRadius;
                        float scaled_hy = hy / currentRadius;

                        // Heart equation: (x^2 + y^2 - 1)^3 - x^2*y^3 = 0
                        // This equation describes a curve. We need to find how "far" a pixel is from this curve.
                        // A simpler approach for graphical effects is to treat the equation's output as a field.
                        // Values close to 0 are "on" the heart curve.
                        float term_sq_sum = scaled_hx * scaled_hx + scaled_hy * scaled_hy;
                        float term_minus_1 = term_sq_sum - 1.0f;
                        
                        float heartValue = powf(term_minus_1, 3.0f) - (scaled_hx * scaled_hx * powf(scaled_hy, 3.0f));
                        
                        // Map heartValue to distToRippleEdge for intensity calculation.
                        // This is an approximation to create a "band" around the heart curve.
                        // `distToRippleEdge` should be 0 when `heartValue` is 0 (on the curve).
                        // It should be `_params.thickness / 2.0f` at the "edge" of the visual band.
                        // `heartEqThreshold` defines how "thick" the heart curve itself is considered.
                        const float heartEqThreshold = 0.03f; // Tunable parameter for heart shape "definition"
                                                              // Smaller values make the line thinner / more sensitive.
                        
                        distToRippleEdge = (fabsf(heartValue) / heartEqThreshold) * (_params.thickness / 2.0f) ;

                    }
                }

                // Common intensity calculation based on distToRippleEdge
                // This pixel is part of the ripple if its distance to the ripple's center-line (distToRippleEdge)
                // is less than half the thickness.
                if (distToRippleEdge < _params.thickness / 2.0f) {
                    // Cosine falloff for smooth edges
                    float baseIntensity = cosf((distToRippleEdge / (_params.thickness / 2.0f)) * ((float)M_PI / 2.0f));
                    // Apply sharpness factor
                    float intensityFactor = powf(baseIntensity, _params.sharpness);
                    intensityFactor = constrain(intensityFactor, 0.0f, 1.0f); // Clamp intensity

                    if (intensityFactor > maxIntensityForThisPixel) {
                        maxIntensityForThisPixel = intensityFactor;
                        colorForThisPixel = HsbColor(_ripples[r_idx].hue, _params.saturation, maxIntensityForThisPixel * _params.baseBrightness);
                    }
                }
            } // End loop over active ripples

            // Set pixel color if any ripple affected it
            if (maxIntensityForThisPixel > 0.0f) {
                int led_index = mapCoordinatesToIndex(x_coord, y_coord); // Use renamed loop variables
                if (led_index >= 0 && led_index < _numLeds) { // Bounds check for safety
                    _strip->SetPixelColor(led_index, colorForThisPixel);
                }
            }
        } // End loop over x_coord
    } // End loop over y_coord
}

// mapCoordinatesToIndex is unchanged from the provided current version
// This version might be specific to a certain 16x16 (4 modules of 8x8) setup.
int RippleEffect::mapCoordinatesToIndex(int x, int y) {
    const int module_width = 8;
    const int module_height = 8;
    const int leds_per_module = module_width * module_height;
    int module_col = x / module_width;
    int module_row = y / module_height;
    int base_index;

    // This mapping seems specific to a 2x2 arrangement of 8x8 modules (total 16x16)
    // And the numbering of these modules (0, 1, 2, 3) might be fixed.
    // module_row=1, module_col=1 -> (bottom-right module by visual layout if 0,0 is top-left) -> base_index = 0
    // module_row=1, module_col=0 -> (bottom-left module) -> base_index = 64
    // module_row=0, module_col=1 -> (top-right module) -> base_index = 128
    // module_row=0, module_col=0 -> (top-left module) -> base_index = 192
    // This order (0, 64, 128, 192) seems to fill from bottom-right, then bottom-left, then top-right, then top-left.
    // And within each module, it's a reverse mapping.

    if (module_row == 1 && module_col == 1) base_index = 0; // Module physically at (8-15, 8-15) maps to LED 0-63
    else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 1; // Module (0-7, 8-15) maps to 64-127
    else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 2; // Module (8-15, 0-7) maps to 128-191
    else base_index = leds_per_module * 3; // Module (0-7, 0-7) maps to 192-255

    int local_x = x % module_width;
    int local_y = y % module_height;

    // Within an 8x8 module, this maps (0,0) local to LED 63 of that module, and (7,7) local to LED 0.
    // (module_height - 1 - local_y) reverses Y (so 0 becomes 7, 7 becomes 0)
    // (module_width - 1 - local_x) reverses X
    int local_offset = (module_height - 1 - local_y) * module_width + (module_width - 1 - local_x);

    int final_index = base_index + local_offset;

    if (final_index >= 0 && final_index < _numLeds) {
        return final_index;
    }
    return -1; // Out of bounds
}