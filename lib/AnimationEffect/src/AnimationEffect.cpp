#include "AnimationEffect.h"
#include <ArduinoJson.h>
#include <cstring> // For strcmp, strcpy
#include "../../../include/DebugUtils.h" // For DEBUG_PRINTLN, DEBUG_PRINTF

// Define the DefaultPreset
const AnimationEffect::Parameters AnimationEffect::DefaultPreset = {
    .prePara = "animated_heart",
    .baseSpeed = 5.0f,
    .baseBrightness = 1.0f // Add this line
};

// Helper function to find an animation by name
const Animation* findAnimationByName(const char* name) {
    if (strcmp(name, "animated_heart") == 0) {
        return &animated_heart_anim; // Assuming animated_heart_anim is defined in MyAnimationData.h
    }
    return nullptr; // Not found
}

AnimationEffect::AnimationEffect() {
    _activeParams = DefaultPreset;
    _targetParams = DefaultPreset;
    _oldParams = DefaultPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    _currentAnimation = findAnimationByName(DefaultPreset.prePara);
    if (_currentAnimation) {
        // Ensure baseSpeed is positive to prevent division by zero or negative duration
        if (DefaultPreset.baseSpeed > 0.0f) {
            _frameDurationMs = 1000.0f / DefaultPreset.baseSpeed;
        } else {
            _frameDurationMs = 1000.0f / 1.0f; // Default to 1 FPS if baseSpeed is invalid
        }
    } else {
        _frameDurationMs = 1000.0f / 1.0f; // Default to 1 FPS if animation not found
        DEBUG_PRINTF("AnimationEffect: Default animation '%s' not found during construction.\n", DefaultPreset.prePara);
    }
    _currentFrameIndex = 0;
    _lastFrameTimeMs = 0; // Will be set properly in Begin or first Update
}

AnimationEffect::~AnimationEffect() {
    // Nothing to dynamically deallocate here
}

// Original complex mapCoordinatesToIndex
int AnimationEffect::mapCoordinatesToIndex(int x, int y) {
    if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) {
         return -1;
    }
    const int module_width = 8;
    const int module_height = 8;
    const int leds_per_module = module_width * module_height;

    int module_col = x / module_width;
    int module_row = y / module_height;

    int base_index;
    // This mapping was from CodeRainEffect, assuming a 2x2 arrangement of 8x8 modules.
    // Module order specific to that setup:
    // (1,1) is top-right in their visual, data starts there.
    // (0,1) is top-left
    // (1,0) is bottom-right
    // (0,0) is bottom-left
    // This was the version from the first submission:
    if (module_row == 1 && module_col == 1) base_index = 0;
    else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 1;
    else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 2;
    else base_index = leds_per_module * 3;

    int local_x = x % module_width;
    int local_y = y % module_height;

    // Pixel wiring within each 8x8 module from CodeRainEffect:
    int local_offset = (module_height - 1 - local_y) * module_width + (module_width - 1 - local_x);

    int final_idx = base_index + local_offset;
    if (final_idx < 0 || final_idx >= _numLeds) { // _numLeds should be 256 for 16x16
        return -1;
    }
    return final_idx;
}

void AnimationEffect::setParameters(const Parameters& params) {
    DEBUG_PRINTLN("AnimationEffect::setParameters(const Parameters&) called.");
    if (_effectInTransition) {
        _oldParams = _activeParams;
    } else {
        _oldParams = _activeParams;
    }
    _targetParams = params;
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    const Animation* newAnim = findAnimationByName(_targetParams.prePara);
    if (newAnim) {
        if (_currentAnimation != newAnim) {
            _currentAnimation = newAnim;
            _currentFrameIndex = 0;
            _lastFrameTimeMs = millis();
        }
    } else {
        DEBUG_PRINTF("AnimationEffect: Animation '%s' not found in setParameters.\n", _targetParams.prePara);
        _currentAnimation = nullptr;
    }
    // Speed change is handled by interpolation in Update()
}

void AnimationEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTF("AnimationEffect::setParameters(json) called with: %s\n", jsonParams);
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error) {
        DEBUG_PRINTF("AnimationEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc["prePara"].is<const char*>()) {
        newParams.prePara = doc["prePara"].as<const char*>();
    }
    if (doc["baseSpeed"].is<float>()) {
        newParams.baseSpeed = doc["baseSpeed"].as<float>();
        if (newParams.baseSpeed <= 0.0f) newParams.baseSpeed = 1.0f;
    }
    if (doc["baseBrightness"].is<float>()) {
        newParams.baseBrightness = doc["baseBrightness"].as<float>();
        // Constrain brightness to 0.0 - 1.0 range
        newParams.baseBrightness = constrain(newParams.baseBrightness, 0.0f, 1.0f);
    }
    setParameters(newParams);
}

void AnimationEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("AnimationEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, DefaultPreset.prePara) == 0 || strcmp(presetName, "default") == 0) {
        setParameters(DefaultPreset);
        DEBUG_PRINTLN("AnimationEffect: Setting DefaultPreset.");
    }
    // Add more presets here if any were defined in original
    else if (strcmp(presetName, "next") == 0) {
        setParameters(DefaultPreset); // Simple "next" logic from original
        DEBUG_PRINTLN("AnimationEffect: 'next' preset called, reloading DefaultPreset.");
    }
    else {
        DEBUG_PRINTF("AnimationEffect: Unknown preset name: %s\n", presetName);
    }
}

void AnimationEffect::Update() {
    if (_effectInTransition) {
        unsigned long currentTimeMs = millis();
        unsigned long elapsedTime = currentTimeMs - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = constrain(t, 0.0f, 1.0f);

        _activeParams.baseSpeed = lerp(_oldParams.baseSpeed, _targetParams.baseSpeed, t);
        _activeParams.baseBrightness = lerp(_oldParams.baseBrightness, _targetParams.baseBrightness, t);

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams;
            _currentAnimation = findAnimationByName(_activeParams.prePara); // Re-find animation post-transition
        }
        // Update frame duration based on possibly new activeParams.baseSpeed
        if (_activeParams.baseSpeed > 0.0f) {
            _frameDurationMs = 1000.0f / _activeParams.baseSpeed;
        } else {
            _frameDurationMs = 1000.0f / 1.0f; // Default to 1 FPS
        }
    }

    if (!_strip || !_currentAnimation || _currentAnimation->frameCount == 0 || _matrixWidth == 0 || _matrixHeight == 0) {
        // If no animation, or strip not set, do nothing or clear.
        // Original version didn't explicitly clear here, relies on main loop or transition handling.
        return;
    }

    unsigned long currentTimeMs = millis();
    if (currentTimeMs - _lastFrameTimeMs >= _frameDurationMs) {
        _lastFrameTimeMs = currentTimeMs;
        _currentFrameIndex = (_currentFrameIndex + 1) % _currentAnimation->frameCount;
    }

    const uint8_t* frameData = _currentAnimation->frames[_currentFrameIndex];
    for (uint8_t y = 0; y < FRAME_HEIGHT; ++y) {
        for (uint8_t x = 0; x < FRAME_WIDTH; ++x) {
            uint16_t pixelDataOffset = (y * FRAME_WIDTH + x) * 3;
            uint8_t r = frameData[pixelDataOffset];
            uint8_t g = frameData[pixelDataOffset + 1];
            uint8_t b = frameData[pixelDataOffset + 2];

            uint8_t final_r = static_cast<uint8_t>(round(r * _activeParams.baseBrightness));
            uint8_t final_g = static_cast<uint8_t>(round(g * _activeParams.baseBrightness));
            uint8_t final_b = static_cast<uint8_t>(round(b * _activeParams.baseBrightness));

            int ledIndex = mapCoordinatesToIndex(x, y);
            if (ledIndex != -1 && ledIndex < _numLeds) {
                _strip->SetPixelColor(ledIndex, RgbColor(final_r, final_g, final_b));
            }
        }
    }
}
