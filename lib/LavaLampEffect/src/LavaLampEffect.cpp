#include "LavaLampEffect.h"
#include <ArduinoJson.h>
#include <math.h>
#include "../../../include/DebugUtils.h" // Corrected path
#include "../../../include/TransitionUtils.h" // For DEFAULT_TRANSITION_DURATION_MS

const LavaLampEffect::Parameters LavaLampEffect::ClassicLavaPreset = {
    .numBlobs = 4,
    .threshold = 1.0f,
    .baseSpeed = 0.8f,
    .baseBrightness = 0.1f,
    .baseColor = "#FF0000", // 红色
    .hueRange = 0.16f,      // 渐变到黄色
    .prePara = "ClassicLava"};

const LavaLampEffect::Parameters LavaLampEffect::MercuryPreset = {
    .numBlobs = 5,
    .threshold = 1.2f,
    .baseSpeed = 1.2f,
    .baseBrightness = 0.1f,
    .baseColor = "#FFFFFF", // 银色/白色
    .hueRange = 0.0f,
    .prePara = "Mercury"};

// 辅助函数实现
void LavaLampEffect::hexToRgb(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hex == nullptr) return;
    // 如果有'#'，跳过它
    if (hex[0] == '#') {
        hex++;
    }
    // 将16进制字符串转换为长整型
    long colorValue = strtol(hex, NULL, 16);
    // 提取R, G, B分量
    r = (colorValue >> 16) & 0xFF;
    g = (colorValue >> 8) & 0xFF;
    b = colorValue & 0xFF;
}

LavaLampEffect::LavaLampEffect()
{
    _activeParams = ClassicLavaPreset;
    _targetParams = ClassicLavaPreset;
    _oldParams = ClassicLavaPreset; // Initialize oldParams as well

    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    // Initialize _internalBaseHue based on the initial preset's baseColor
    uint8_t r, g, b;
    hexToRgb(_activeParams.baseColor, r, g, b); // Use private member function
    RgbColor rgb_color_temp(r, g, b); // Temporary RgbColor object
    HsbColor hsb_color_temp(rgb_color_temp); // Temporary HsbColor object
    _activeInternalBaseHue = hsb_color_temp.H;
    _targetInternalBaseHue = _activeInternalBaseHue;
    _oldInternalBaseHue = _activeInternalBaseHue;

    _blobs = nullptr;
    _lastUpdateTime = 0; // Initialize _lastUpdateTime
}

LavaLampEffect::~LavaLampEffect()
{
    if (_blobs)
        delete[] _blobs;
}

void LavaLampEffect::setParameters(const Parameters& params) {
    DEBUG_PRINTLN("LavaLampEffect::setParameters(const Parameters&) called.");

    _oldParams = _activeParams;
    _targetParams = params;

    bool needsInitBlobs = false;

    // Handle numBlobs change (instant)
    // Compare new target with current active number of blobs
    if (_targetParams.numBlobs != _activeParams.numBlobs || _blobs == nullptr) {
        DEBUG_PRINTF("LavaLampEffect: numBlobs changing from %d to %d\n", _activeParams.numBlobs, _targetParams.numBlobs);
        // Update _activeParams.numBlobs for initBlobs if it's going to be called.
        // This ensures initBlobs allocates and initializes based on the new count.
        _activeParams.numBlobs = _targetParams.numBlobs;
        needsInitBlobs = true;
        // Align _oldParams.numBlobs to prevent interpolation of numBlobs count itself.
        _oldParams.numBlobs = _targetParams.numBlobs;
    }

    // Handle baseColor string change (instant for string part)
    // and prepare internal hue for transition.
    // Compare new target baseColor with current active baseColor.
    // Also check if _activeParams.baseColor is null (can happen if not initialized properly before first call)
    if (_activeParams.baseColor == nullptr || strcmp(_targetParams.baseColor, _activeParams.baseColor) != 0) {
        DEBUG_PRINTF("LavaLampEffect: baseColor string changing from %s to %s\n", (_activeParams.baseColor ? _activeParams.baseColor : "null"), _targetParams.baseColor);
        _activeParams.baseColor = _targetParams.baseColor; // String changes instantly.
        _oldParams.baseColor = _targetParams.baseColor;   // Align old to prevent string "lerp" attempt.

        uint8_t r, g, b;
        hexToRgb(_activeParams.baseColor, r, g, b);
        RgbColor rgb_color_temp(r, g, b);
        HsbColor hsb_color_temp(rgb_color_temp);

        _oldInternalBaseHue = _activeInternalBaseHue; // Capture current internal hue for transition start.
        _targetInternalBaseHue = hsb_color_temp.H;    // Set new target for internal hue.
        DEBUG_PRINTF("Internal base hue target set to: %.2f (transition from %.2f)\n", _targetInternalBaseHue, _oldInternalBaseHue);
    } else {
        // If baseColor string didn't change, internal hue target remains the same as current active.
        _targetInternalBaseHue = _activeInternalBaseHue;
        // _oldInternalBaseHue is already _activeInternalBaseHue if no transition was ongoing or just finished.
        // If a transition was ongoing for hue, _oldInternalBaseHue already holds the start of that.
        // This line ensures that if only other params change, hue doesn't try to "transition" to itself.
         _oldInternalBaseHue = _activeInternalBaseHue;
    }

    // Update prePara instantly
    if (_targetParams.prePara != _oldParams.prePara) { // Compare against old to see if it's a new target
        _activeParams.prePara = _targetParams.prePara; // Instant change
        _oldParams.prePara = _targetParams.prePara;    // Align old
    }

    // Call initBlobs if numBlobs changed.
    // initBlobs uses _activeParams.numBlobs and _activeParams.baseSpeed.
    // _activeParams.baseSpeed will be its current (potentially old) value here.
    // The transition logic in Update() will then lerp _activeParams.baseSpeed towards _targetParams.baseSpeed.
    // This means newly initialized blobs might start with the old speed and then their behavior changes as speed lerps.
    if (needsInitBlobs) {
        initBlobs();
    }

    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    DEBUG_PRINTLN("LavaLampEffect transition started.");
}

void LavaLampEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("LavaLampEffect::setParameters(json) called.");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("LavaLampEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    // Start with current target or active params to allow partial updates
    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc.containsKey("numBlobs")) newParams.numBlobs = doc["numBlobs"].as<uint8_t>();
    if (doc.containsKey("threshold")) newParams.threshold = doc["threshold"].as<float>();
    if (doc.containsKey("baseSpeed")) newParams.baseSpeed = doc["baseSpeed"].as<float>();
    if (doc.containsKey("baseBrightness")) newParams.baseBrightness = doc["baseBrightness"].as<float>();

    // Special handling for baseColor to ensure it points to a persistent string if prePara matches
    bool preParaChangedAndMatched = false; // Flag to see if a known preset was fully applied
    if (doc.containsKey("prePara")) {
        const char* presetStr = doc["prePara"].as<const char*>();
        if (presetStr) {
            if (strcmp(presetStr, ClassicLavaPreset.prePara) == 0) {
                newParams = ClassicLavaPreset; // Copy entire preset
                preParaChangedAndMatched = true;
            } else if (strcmp(presetStr, MercuryPreset.prePara) == 0) {
                newParams = MercuryPreset; // Copy entire preset
                preParaChangedAndMatched = true;
            } else {
                // If prePara is some custom string, assign it.
                // User is responsible for lifetime of this string if it's not a literal.
                newParams.prePara = presetStr;
            }
        }
    }

    // Only parse baseColor from JSON if a known preset wasn't just fully applied.
    // If a preset was applied, newParams.baseColor is already correct.
    // If prePara was custom or not present, then parse baseColor if it exists.
    if (doc.containsKey("baseColor") && !preParaChangedAndMatched) {
        // This assignment is risky if doc["baseColor"] points to a temporary string in the JSON doc.
        // For safety, this should ideally only happen if prePara is also being set to a custom value,
        // or if Parameters.baseColor becomes a String type.
        // For now, we follow the original pattern of direct assignment for non-preset baseColor.
        newParams.baseColor = doc["baseColor"].as<const char*>(); // Potentially risky if JSON string is not long-lived
    }

    if (doc.containsKey("hueRange")) newParams.hueRange = doc["hueRange"].as<float>();

    setParameters(newParams); // Call the struct version to handle logic and transition
}

void LavaLampEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("LavaLampEffect::setPreset called with: %s\n", presetName);
    const char* currentEffectivePresetName = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;
    if (currentEffectivePresetName == nullptr) currentEffectivePresetName = ClassicLavaPreset.prePara; // Fallback

    if (strcmp(presetName, "next") == 0) {
        if (strcmp(currentEffectivePresetName, ClassicLavaPreset.prePara) == 0) {
            setParameters(MercuryPreset);
        } else {
            setParameters(ClassicLavaPreset);
        }
    } else if (strcmp(presetName, ClassicLavaPreset.prePara) == 0) {
        setParameters(ClassicLavaPreset);
    } else if (strcmp(presetName, MercuryPreset.prePara) == 0) {
        setParameters(MercuryPreset);
    } else {
        DEBUG_PRINTF("Unknown preset name in LavaLampEffect::setPreset: %s\n", presetName);
    }
    // The setParameters call will issue its own DEBUG_PRINTLN.
}

void LavaLampEffect::initBlobs()
{
    // This function is called when numBlobs changes (using _activeParams.numBlobs from setParameters)
    // or during initial Begin (where _activeParams is set by constructor).
    // It should use _activeParams for initializing blob properties.
    for (int i = 0; i < _activeParams.numBlobs; ++i)
    {
        _blobs[i].x = random(0, _matrixWidth * 100) / 100.0f;
        _blobs[i].y = random(0, _matrixHeight * 100) / 100.0f;
        _blobs[i].vx = (random(0, 200) - 100) / 100.0f * _activeParams.baseSpeed;
        _blobs[i].vy = (random(0, 200) - 100) / 100.0f * _activeParams.baseSpeed;
        _blobs[i].radius = random(150, 250) / 100.0f; // 半径在 1.5 到 2.5 之间
    }
}

int LavaLampEffect::mapCoordinatesToIndex(int x, int y) {
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

void LavaLampEffect::Update()
{
    if (_effectInTransition) {
        unsigned long currentTimeMs_lerp = millis(); // Renamed to avoid conflict with existing currentTimeMs
        unsigned long elapsedTime = currentTimeMs_lerp - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        // Interpolate parameters (numBlobs and baseColor string change instantly and are handled in setParameters)
        _activeParams.threshold = lerp(_oldParams.threshold, _targetParams.threshold, t);
        _activeParams.baseSpeed = lerp(_oldParams.baseSpeed, _targetParams.baseSpeed, t);
        _activeParams.baseBrightness = lerp(_oldParams.baseBrightness, _targetParams.baseBrightness, t);
        _activeParams.hueRange = lerp(_oldParams.hueRange, _targetParams.hueRange, t);
        _activeInternalBaseHue = lerp(_oldInternalBaseHue, _targetInternalBaseHue, t); // Transition the internal HSB Hue value

        if (t >= 1.0f) {
            _effectInTransition = false;
            // Ensure all parts of _activeParams match _targetParams at the end
            _activeParams.threshold = _targetParams.threshold;
            _activeParams.baseSpeed = _targetParams.baseSpeed;
            _activeParams.baseBrightness = _targetParams.baseBrightness;
            _activeParams.hueRange = _targetParams.hueRange;
            _activeInternalBaseHue = _targetInternalBaseHue; // Ensure internal hue matches target
            // numBlobs, baseColor string, and prePara were already set to target values instantly
            _activeParams.numBlobs = _targetParams.numBlobs;
            _activeParams.baseColor = _targetParams.baseColor;
            _activeParams.prePara = _targetParams.prePara;
            DEBUG_PRINTLN("LavaLampEffect transition complete.");
        }
    }

    if (!_strip || !_blobs)
        return;

    unsigned long currentTime = millis();
    float deltaTime = (currentTime - _lastUpdateTime) / 1000.0f;
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;
    _lastUpdateTime = currentTime;

    for (int i = 0; i < _activeParams.numBlobs; ++i)
    {
        Metaball &blob = _blobs[i];
        blob.x += blob.vx * deltaTime * _activeParams.baseSpeed;
        blob.y += blob.vy * deltaTime * _activeParams.baseSpeed;

        if (blob.x < 0 || blob.x > _matrixWidth - 1)
            blob.vx *= -1;
        if (blob.y < 0 || blob.y > _matrixHeight - 1)
            blob.vy *= -1;
    }

    _strip->ClearTo(RgbColor(0));

    for (int py = 0; py < _matrixHeight; ++py)
    {
        for (int px = 0; px < _matrixWidth; ++px)
        {
            float totalEnergy = 0.0f;
            float pixelCenterX = px + 0.5f;
            float pixelCenterY = py + 0.5f;

            for (int i = 0; i < _activeParams.numBlobs; ++i)
            {
                Metaball &blob = _blobs[i];
                float dx = pixelCenterX - blob.x;
                float dy = pixelCenterY - blob.y;
                float distSq = dx * dx + dy * dy;
                if (distSq == 0.0f)
                    distSq = 0.0001f;

                totalEnergy += (blob.radius * blob.radius) / distSq;
            }

            if (totalEnergy > _activeParams.threshold)
            {
                float excessEnergy = totalEnergy - _activeParams.threshold;
                float brightness = constrain(excessEnergy * 0.5f, 0.0f, 1.0f) * _activeParams.baseBrightness;
                float hue = _activeInternalBaseHue + constrain(excessEnergy * 0.2f, 0.0f, 1.0f) * _activeParams.hueRange;

                float saturation = 1.0f;
                if (strcmp(_activeParams.prePara, "Mercury") == 0)
                {
                    saturation = 0.0f; // 银色就是无饱和度的白色
                }

                HsbColor color(hue, saturation, brightness);

                int led_index = mapCoordinatesToIndex(px, py);
                if (led_index >= 0 && led_index < _numLeds) {
                    _strip->SetPixelColor(led_index, color);
                }
            }
        }
    }
}