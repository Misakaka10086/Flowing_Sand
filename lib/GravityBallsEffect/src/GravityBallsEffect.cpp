#include "GravityBallsEffect.h"
#include <ArduinoJson.h>
#include "../../../include/DebugUtils.h" // For DEBUG_PRINTF, DEBUG_PRINTLN
// 定义和初始化静态预设
const GravityBallsEffect::Parameters GravityBallsEffect::BouncyPreset = {
    .numBalls = 15,
    .gravityScale = 25.0f,
    .dampingFactor = 0.95f,
    .sensorDeadZone = 0.8f,
    .restitution = 0.75f,
    .baseBrightness = 80,
    .brightnessCyclePeriodS = 3.0f,
    .minBrightnessScale = 0.2f,
    .maxBrightnessScale = 1.0f,
    .colorCyclePeriodS = 10.0f,
    .ballColorSaturation = 1.0f,
    .prePara = "Bouncy"
};

const GravityBallsEffect::Parameters GravityBallsEffect::PlasmaPreset = {
    .numBalls = 30,         // 更多的球
    .gravityScale = 40.0f,  // 对重力更敏感
    .dampingFactor = 0.98f, // 阻尼更小，更"滑溜"
    .sensorDeadZone = 1.0f,
    .restitution = 0.4f, // 弹性更小，像粘稠的等离子体
    .baseBrightness = 120,
    .brightnessCyclePeriodS = 5.0f,
    .minBrightnessScale = 0.4f,
    .maxBrightnessScale = 1.0f,
    .colorCyclePeriodS = 15.0f, // 颜色变化更慢
    .ballColorSaturation = 0.8f,
    .prePara = "Plasma"
};

GravityBallsEffect::GravityBallsEffect()
{
    _activeParams = BouncyPreset;
    _targetParams = BouncyPreset;
    _oldParams = BouncyPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS; // From TransitionUtils.h
    _effectTransitionStartTimeMs = 0;

    _balls = nullptr;
    _accel = nullptr;
    _strip = nullptr;
    // The actual initialization of hardware (accelerometer) and strip
    // is handled in Begin(). Ball allocation (initBalls) is also called in Begin()
    // or when parameters change.
}

GravityBallsEffect::~GravityBallsEffect()
{
    if (_balls != nullptr)
        delete[] _balls;
    if (_accel != nullptr)
        delete _accel;
}

void GravityBallsEffect::setParameters(const Parameters &params)
{
    DEBUG_PRINTLN("GravityBallsEffect::setParameters(const Parameters&) called.");

    // Capture the currently active parameters as the starting point for the transition.
    _oldParams = _activeParams;

    // Copy incoming params to a mutable structure to potentially adjust numBalls before full assignment.
    Parameters newTarget = params;

    // Handle numBalls change immediately as it affects memory allocation.
    if (newTarget.numBalls != _activeParams.numBalls) {
        DEBUG_PRINTF("GravityBalls: numBalls changing from %d to %d\n", _activeParams.numBalls, newTarget.numBalls);
        // Set _targetParams.numBalls specifically for initBalls, as initBalls uses _targetParams.
        _targetParams.numBalls = newTarget.numBalls;
        initBalls(); // Re-initialize balls with _targetParams.numBalls.

        // Reflect the change in numBalls immediately in active and old params to avoid interpolation of numBalls.
        _activeParams.numBalls = _targetParams.numBalls;
        _oldParams.numBalls = _targetParams.numBalls;
    }

    // Assign the full set of target parameters.
    _targetParams = newTarget;

    // Start the transition for other parameters.
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    DEBUG_PRINTLN("GravityBallsEffect transition started.");
}

// ***** 新增的重载 setParameters(json) 方法 *****
void GravityBallsEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("GravityBallsEffect::setParameters(json) called [refactored].");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("GravityBallsEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;
    
    // Parse all fields using .is<T>()
    if (doc["numBalls"].is<uint8_t>()) newParams.numBalls = doc["numBalls"].as<uint8_t>();
    if (doc["gravityScale"].is<float>()) newParams.gravityScale = doc["gravityScale"].as<float>();
    if (doc["dampingFactor"].is<float>()) newParams.dampingFactor = doc["dampingFactor"].as<float>();
    if (doc["sensorDeadZone"].is<float>()) newParams.sensorDeadZone = doc["sensorDeadZone"].as<float>();
    if (doc["restitution"].is<float>()) newParams.restitution = doc["restitution"].as<float>();
    if (doc["baseBrightness"].is<uint8_t>()) newParams.baseBrightness = doc["baseBrightness"].as<uint8_t>();
    if (doc["brightnessCyclePeriodS"].is<float>()) newParams.brightnessCyclePeriodS = doc["brightnessCyclePeriodS"].as<float>();
    if (doc["minBrightnessScale"].is<float>()) newParams.minBrightnessScale = doc["minBrightnessScale"].as<float>();
    if (doc["maxBrightnessScale"].is<float>()) newParams.maxBrightnessScale = doc["maxBrightnessScale"].as<float>();
    if (doc["colorCyclePeriodS"].is<float>()) newParams.colorCyclePeriodS = doc["colorCyclePeriodS"].as<float>();
    if (doc["ballColorSaturation"].is<float>()) newParams.ballColorSaturation = doc["ballColorSaturation"].as<float>();

    if (doc["prePara"].is<const char*>()) {
        const char* presetStr = doc["prePara"].as<const char*>();
        if (presetStr) { // Check if conversion was successful
            if (strcmp(presetStr, BouncyPreset.prePara) == 0) {
                newParams.prePara = BouncyPreset.prePara;
            } else if (strcmp(presetStr, PlasmaPreset.prePara) == 0) {
                newParams.prePara = PlasmaPreset.prePara;
            }
        }
    }
    
    // Call the struct version to handle all logic including numBalls change and transition
    setParameters(newParams);
}

void GravityBallsEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("GravityBallsEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, "next") == 0) {
        // Determine the current preset based on target if transitioning, otherwise active.
        const char* currentEffectivePresetName = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;

        // Fallback if prePara is somehow null (e.g., if not set initially).
        if (currentEffectivePresetName == nullptr) {
            currentEffectivePresetName = BouncyPreset.prePara;
        }

        if (strcmp(currentEffectivePresetName, BouncyPreset.prePara) == 0) {
            setParameters(PlasmaPreset); // This will trigger a transition.
            DEBUG_PRINTLN("Switching to PlasmaPreset via 'next'");
        } else {
            setParameters(BouncyPreset); // This will trigger a transition.
            DEBUG_PRINTLN("Switching to BouncyPreset via 'next'");
        }
    } else if (strcmp(presetName, BouncyPreset.prePara) == 0) {
        setParameters(BouncyPreset); // Triggers transition.
        DEBUG_PRINTLN("Setting BouncyPreset");
    } else if (strcmp(presetName, PlasmaPreset.prePara) == 0) {
        setParameters(PlasmaPreset); // Triggers transition.
        DEBUG_PRINTLN("Setting PlasmaPreset");
    } else {
        DEBUG_PRINTF("Unknown preset name in GravityBallsEffect::setPreset: %s\n", presetName);
    }
}

void GravityBallsEffect::initBalls()
{
    if (_balls != nullptr)
    {
        delete[] _balls;
    }
    // Allocate based on _targetParams.numBalls as this function is called when target changes.
    _balls = new Ball[_targetParams.numBalls];

    float ballRadius = 0.5f;
    float minSeparationDistSq = (2 * ballRadius) * (2 * ballRadius);

    for (int i = 0; i < _targetParams.numBalls; ++i)
    {
        bool positionOK;
        do
        {
            positionOK = true;
            _balls[i].x = random(_matrixWidth * 100) / 100.0f;
            _balls[i].y = random(_matrixHeight * 100) / 100.0f;
            for (int j = 0; j < i; ++j)
            {
                float dx_init = _balls[i].x - _balls[j].x;
                float dy_init = _balls[i].y - _balls[j].y;
                if (dx_init * dx_init + dy_init * dy_init < minSeparationDistSq)
                {
                    positionOK = false;
                    break;
                }
            }
        } while (!positionOK);

        _balls[i].vx = 0;
        _balls[i].vy = 0;
        _balls[i].brightnessPhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        // Initialize with target parameters
        _balls[i].brightnessFactor = _targetParams.minBrightnessScale;
        _balls[i].huePhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        _balls[i].hue = fmod(((0.0f / 1000.0f * 2.0f * PI / _targetParams.colorCyclePeriodS) + _balls[i].huePhaseOffset) / (2.0f * PI), 1.0f);
        if (_balls[i].hue < 0)
            _balls[i].hue += 1.0f;
    }
}

int GravityBallsEffect::mapCoordinatesToIndex(int x, int y) {
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

void GravityBallsEffect::Update()
{
    if (_effectInTransition) {
        unsigned long currentTimeMs = millis();
        unsigned long elapsedTime = currentTimeMs - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        // Interpolate parameters (numBalls is handled separately and changes instantly)
        _activeParams.gravityScale = lerp(_oldParams.gravityScale, _targetParams.gravityScale, t);
        _activeParams.dampingFactor = lerp(_oldParams.dampingFactor, _targetParams.dampingFactor, t);
        _activeParams.sensorDeadZone = lerp(_oldParams.sensorDeadZone, _targetParams.sensorDeadZone, t);
        _activeParams.restitution = lerp(_oldParams.restitution, _targetParams.restitution, t);
        _activeParams.baseBrightness = static_cast<uint8_t>(lerp(static_cast<int>(_oldParams.baseBrightness), static_cast<int>(_targetParams.baseBrightness), t));
        _activeParams.brightnessCyclePeriodS = lerp(_oldParams.brightnessCyclePeriodS, _targetParams.brightnessCyclePeriodS, t);
        _activeParams.minBrightnessScale = lerp(_oldParams.minBrightnessScale, _targetParams.minBrightnessScale, t);
        _activeParams.maxBrightnessScale = lerp(_oldParams.maxBrightnessScale, _targetParams.maxBrightnessScale, t);
        _activeParams.colorCyclePeriodS = lerp(_oldParams.colorCyclePeriodS, _targetParams.colorCyclePeriodS, t);
        _activeParams.ballColorSaturation = lerp(_oldParams.ballColorSaturation, _targetParams.ballColorSaturation, t);
        // _activeParams.prePara changes with _targetParams at end of transition

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams; // Ensure exact match at the end
            DEBUG_PRINTLN("GravityBallsEffect transition complete.");
        }
    }

    if (_strip == nullptr || _accel == nullptr || _balls == nullptr)
        return;

    unsigned long currentTime = millis();
    float dt = (currentTime - _lastUpdateTime) / 1000.0f;
    if (dt <= 0.0001f)
        dt = 0.001f;
    _lastUpdateTime = currentTime;
    float totalTimeSeconds = currentTime / 1000.0f;

    sensors_event_t event;
    _accel->getEvent(&event);
    float rawAx = event.acceleration.x;
    float rawAz = event.acceleration.z;

    float ax_eff = (abs(rawAx) < _activeParams.sensorDeadZone) ? 0 : rawAx;
    float az_eff = (abs(rawAz) < _activeParams.sensorDeadZone) ? 0 : rawAz;

    float forceX = -ax_eff * _activeParams.gravityScale;
    float forceY = -az_eff * _activeParams.gravityScale;

    // Update balls using parameters from _activeParams
    for (int i = 0; i < _activeParams.numBalls; ++i)
    {
        float sinWaveBright = (sin((2.0f * PI / _activeParams.brightnessCyclePeriodS * totalTimeSeconds) + _balls[i].brightnessPhaseOffset) + 1.0f) / 2.0f;
        _balls[i].brightnessFactor = _activeParams.minBrightnessScale + sinWaveBright * (_activeParams.maxBrightnessScale - _activeParams.minBrightnessScale);
        float rawHueAngle = (2.0f * PI / _activeParams.colorCyclePeriodS * totalTimeSeconds) + _balls[i].huePhaseOffset;
        _balls[i].hue = fmod(rawHueAngle / (2.0f * PI), 1.0f);
        if (_balls[i].hue < 0.0f)
            _balls[i].hue += 1.0f;

        _balls[i].vx += forceX * dt;
        _balls[i].vy += forceY * dt;
        _balls[i].vx *= _activeParams.dampingFactor;
        _balls[i].vy *= _activeParams.dampingFactor;
        _balls[i].x += _balls[i].vx * dt;
        _balls[i].y += _balls[i].vy * dt;

        float ballRadius = 0.5f;
        if (_balls[i].x < ballRadius)
        {
            _balls[i].x = ballRadius;
            _balls[i].vx *= -_activeParams.restitution;
        }
        else if (_balls[i].x > _matrixWidth - ballRadius)
        {
            _balls[i].x = _matrixWidth - ballRadius;
            _balls[i].vx *= -_activeParams.restitution;
        }
        if (_balls[i].y < ballRadius)
        {
            _balls[i].y = ballRadius;
            _balls[i].vy *= -_activeParams.restitution;
        }
        else if (_balls[i].y > _matrixHeight - ballRadius)
        {
            _balls[i].y = _matrixHeight - ballRadius;
            _balls[i].vy *= -_activeParams.restitution;
        }
    }

    float ballRadius = 0.5f;
    float minSeparationDistSq = (2 * ballRadius) * (2 * ballRadius);
    float invBallMass = 1.0f;
    for (int i = 0; i < _activeParams.numBalls; ++i)
    {
        for (int j = i + 1; j < _activeParams.numBalls; ++j)
        {
            float dx = _balls[j].x - _balls[i].x;
            float dy = _balls[j].y - _balls[i].y;
            float distSq = dx * dx + dy * dy;
            if (distSq < minSeparationDistSq && distSq > 0.00001f)
            {
                float dist = sqrt(distSq);
                float nx = dx / dist;
                float ny = dy / dist;
                float rvx = _balls[j].vx - _balls[i].vx;
                float rvy = _balls[j].vy - _balls[i].vy;
                float velAlongNormal = rvx * nx + rvy * ny;
                if (velAlongNormal < 0)
                {
                    float impulseMagnitude = -(1.0f + _activeParams.restitution) * velAlongNormal / (2.0f * invBallMass);
                    _balls[i].vx -= impulseMagnitude * nx * invBallMass;
                    _balls[i].vy -= impulseMagnitude * ny * invBallMass;
                    _balls[j].vx += impulseMagnitude * nx * invBallMass;
                    _balls[j].vy += impulseMagnitude * ny * invBallMass;
                }
                float overlap = (2 * ballRadius) - dist;
                _balls[i].x -= nx * overlap * 0.5f;
                _balls[i].y -= ny * overlap * 0.5f;
                _balls[j].x += nx * overlap * 0.5f;
                _balls[j].y += ny * overlap * 0.5f;
            }
        }
    }

    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int i = 0; i < _activeParams.numBalls; ++i)
    {
        int pixelX = round(_balls[i].x - ballRadius);
        int pixelY = round(_balls[i].y - ballRadius);
        pixelX = constrain(pixelX, 0, _matrixWidth - 1);
        pixelY = constrain(pixelY, 0, _matrixHeight - 1);
        int ledIndex = mapCoordinatesToIndex(pixelX, pixelY);
        if (ledIndex >= 0 && ledIndex < _numLeds)
        {
            HsbColor hsbColor(_balls[i].hue, _activeParams.ballColorSaturation, _balls[i].brightnessFactor * (_activeParams.baseBrightness / 255.0f));
            _strip->SetPixelColor(ledIndex, hsbColor);
        }
    }
}