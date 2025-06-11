#include "AnimationEffect.h"
#include <ArduinoJson.h>
#include <cstring> // For strcmp, strcpy
#include "../../../include/DebugUtils.h" // For DEBUG_PRINTLN, DEBUG_PRINTF

// Define the DefaultPreset
// Assuming 'animated_heart_anim' is globally available from MyAnimationData.h
// We need to select an animation by name. For now, we'll assume a way to map names to Animation instances.
// Let's make prePara a const char* that holds the name of the animation.
const AnimationEffect::Parameters AnimationEffect::DefaultPreset = {
    .prePara = "animated_heart", // This name will need to be resolved to the actual animation
    .baseSpeed = 5.0f            // 5 frames per second
};

// Helper function to find an animation by name
// This is a simple implementation. A more robust solution might involve a map or a list of registered animations.
const Animation* findAnimationByName(const char* name) {
    if (strcmp(name, "animated_heart") == 0) {
        return &animated_heart_anim; // Assuming animated_heart_anim is defined in MyAnimationData.h
    }
    // Add other animations here
    // else if (strcmp(name, "another_anim") == 0) {
    //     return &another_anim_data;
    // }
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
        _frameDurationMs = 1000.0f / DefaultPreset.baseSpeed;
    } else {
        _frameDurationMs = 1000.0f / 1.0f; // Default to 1 FPS if animation not found
        DEBUG_PRINTF("AnimationEffect: Default animation '%s' not found.\n", DefaultPreset.prePara);
    }
    _currentFrameIndex = 0;
    _lastFrameTimeMs = 0;
}

AnimationEffect::~AnimationEffect() {
    // Nothing to dynamically deallocate here unless _strip was owned, which it is not.
}

// This coordinate mapping is copied from CodeRainEffect and might need adjustment
// based on the LED matrix wiring. The problem description mentions:
// "像素排列顺序：建议使用行优先，从左上至右下（可适配实际 wiring 顺序）"
// This function assumes a specific panel layout.
// For a simple 16x16 matrix that is wired linearly row by row:
// int AnimationEffect::mapCoordinatesToIndex(int x, int y) {
//     if (x < 0 || x >= _matrixWidth || y < 0 || y >= _matrixHeight) {
//         return -1; // Out of bounds
//     }
//     return y * _matrixWidth + x;
// }
// Let's use the more complex one from CodeRain for now, assuming a similar setup.
int AnimationEffect::mapCoordinatesToIndex(int x, int y) {
    // Ensure coordinates are within the 16x16 matrix for this effect
    if (x < 0 || x >= FRAME_WIDTH || y < 0 || y >= FRAME_HEIGHT) {
         //DEBUG_PRINTF("Coord out of bounds: (%d, %d)\n", x, y);
         return -1;
    }

    // The physical display is 64x4, but effects might operate on logical sub-matrices.
    // This effect targets a 16x16 logical matrix.
    // We need to map this 16x16 logical matrix to a portion of the 64x4 physical display
    // or assume the EffectController handles the full strip and this effect only updates its part.
    // For now, assume this effect is given a _matrixWidth and _matrixHeight that corresponds
    // to the physical part of the strip it should control.
    // If _matrixWidth is 16 and _matrixHeight is 16, and it's a direct mapping:
    // return y * _matrixWidth + x; // Simple row-major mapping

    // Using the provided complex mapping (adjust if your 16x16 matrix is wired differently or positioned differently on a larger display)
    // This mapping seems to be for a 4-panel display of 8x8 modules each, totaling 16x16.
    // The total number of LEDs is 256 (16*16).
    // The mapping needs to be correct for YOUR specific 16x16 LED arrangement.
    // If your 16x16 matrix is a single, linear strip of 256 LEDs wired row by row:
    // return y * 16 + x;

    // If it's the specific 4-module layout from CodeRainEffect (totaling 16x16 from 4 8x8 modules):
    const int module_width = 8;
    const int module_height = 8;
    const int leds_per_module = module_width * module_height; // 64

    int module_col = x / module_width;  // 0 or 1
    int module_row = y / module_height; // 0 or 1

    int base_index;

    // This specific mapping is for a 2x2 arrangement of 8x8 modules.
    // Module order:
    // Module (0,1) (top-left by visual, but data cable might start at (1,1)) | Module (1,1) (top-right)
    // -----------------------------------------------------------------------|---------------------------
    // Module (0,0) (bottom-left)                                           | Module (1,0) (bottom-right)
    //
    // The CodeRainEffect mapping was:
    // if (module_row == 1 && module_col == 1) base_index = 0; // Top-right module in their setup ( visually module [1][1] )
    // else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 1; // Top-left ( visually module [0][1] )
    // else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 2; // Bottom-right ( visually module [1][0] )
    // else base_index = leds_per_module * 3; // Bottom-left ( visually module [0][0] )

    // Let's assume a more standard top-left origin for module indexing for now for a 2x2 setup
    // Visual layout:
    // Mod(0,0) | Mod(1,0)
    //----------|----------
    // Mod(0,1) | Mod(1,1)
    // And data flows Mod(0,0) -> Mod(1,0) -> Mod(0,1) -> Mod(1,1)

    if (module_row == 0 && module_col == 0) base_index = 0; // Top-left visual module
    else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 1; // Top-right visual module
    else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 2; // Bottom-left visual module
    else base_index = leds_per_module * 3; // Bottom-right visual module


    int local_x = x % module_width;
    int local_y = y % module_height;

    // Pixel wiring within each 8x8 module: row major, from top-left, but reversed y for each row
    // and then reversed x for the whole module.
    // This is from the original CodeRainEffect, it might be specific to their hardware.
    // int local_offset = (module_height - 1 - local_y) * module_width + (module_width - 1 - local_x);

    // Let's assume a simpler wiring for now: standard row-major within each module.
    // (0,0) (0,1) ... (0,7)
    // (1,0) (1,1) ... (1,7)
    // ...
    // (7,0) (7,1) ... (7,7)
    int local_offset = local_y * module_width + local_x;


    int final_idx = base_index + local_offset;
    if (final_idx < 0 || final_idx >= _numLeds) {
        // DEBUG_PRINTF("Final index out of bounds: %d (x:%d y:%d mR:%d mC:%d lX:%d lY:%d bI:%d lO:%d)\n", final_idx, x,y,module_row, module_col, local_x, local_y, base_index, local_offset);
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
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS; // Could be part of params too

    // Update animation pointer and frame duration based on target parameters
    const Animation* newAnim = findAnimationByName(_targetParams.prePara);
    if (newAnim) {
        // If animation changes, reset frame index
        if (_currentAnimation != newAnim) {
            _currentAnimation = newAnim;
            _currentFrameIndex = 0;
            _lastFrameTimeMs = millis(); // Reset timer to show first frame immediately
        }
        // Update speed, this will be picked up by transition or directly by Update
        // _frameDurationMs = 1000.0f / _targetParams.baseSpeed; // This will be interpolated
    } else {
        DEBUG_PRINTF("AnimationEffect: Animation '%s' not found in setParameters.\n", _targetParams.prePara);
        // Optionally, keep the old animation or stop
        // _currentAnimation = nullptr; // Or keep _oldParams._currentAnimation
    }
    DEBUG_PRINTLN("AnimationEffect transition started.");
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
        // Note: prePara is a const char*. If ArduinoJson duplicates the string, it's fine.
        // If it points into its internal buffer, ensure lifetime or copy it.
        // For this setup, EffectController usually passes string literals or char arrays with sufficient lifetime.
    }
    if (doc["baseSpeed"].is<float>()) {
        newParams.baseSpeed = doc["baseSpeed"].as<float>();
        if (newParams.baseSpeed <= 0) newParams.baseSpeed = 1.0f; // Prevent division by zero or negative speed
    }

    // Example for adding more parameters:
    // if (doc["brightness"].is<float>()) newParams.brightness = doc["brightness"].as<float>();

    setParameters(newParams);
}

void AnimationEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("AnimationEffect::setPreset called with: %s\n", presetName);
    if (strcmp(presetName, DefaultPreset.prePara) == 0 || strcmp(presetName, "default") == 0) {
        setParameters(DefaultPreset);
        DEBUG_PRINTLN("AnimationEffect: Setting DefaultPreset.");
    }
    // Add more presets here
    // else if (strcmp(presetName, "AnotherPreset") == 0) {
    //    setParameters(AnotherPresetObject);
    // }
    else if (strcmp(presetName, "next") == 0) {
        // Simple "next" logic: if there's only one preset, it just reloads it.
        // If more presets are added, this logic should cycle through them.
        setParameters(DefaultPreset);
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
        // prePara is not interpolated, it changes when _targetParams is set and findAnimationByName is called.
        // If the animation name itself changes, _currentAnimation is updated in setParameters.

        if (t >= 1.0f) {
            _effectInTransition = false;
            _activeParams = _targetParams; // Ensure exact match at the end
            // Update animation and speed based on final target parameters
            _currentAnimation = findAnimationByName(_activeParams.prePara);
            if (_currentAnimation) {
                if (_activeParams.baseSpeed > 0) {
                    _frameDurationMs = 1000.0f / _activeParams.baseSpeed;
                } else {
                    _frameDurationMs = 1000.0f / 1.0f; // Default to 1 FPS if speed is invalid
                }
            } else {
                 _frameDurationMs = 1000.0f / 1.0f;
                 DEBUG_PRINTF("AnimationEffect: Animation '%s' not found after transition.\n", _activeParams.prePara);
            }
            DEBUG_PRINTLN("AnimationEffect transition complete.");
        } else {
            // While transitioning, update frame duration based on interpolated speed
            if (_activeParams.baseSpeed > 0) {
                 _frameDurationMs = 1000.0f / _activeParams.baseSpeed;
            } else {
                 _frameDurationMs = 1000.0f / 1.0f;
            }
        }
    }


    if (!_strip || !_currentAnimation || _currentAnimation->frameCount == 0 || _matrixWidth == 0 || _matrixHeight == 0) {
        if (!_currentAnimation) {
            // DEBUG_PRINTLN("AnimationEffect: No current animation to play.");
        }
        // Optionally clear the strip if no animation
        // _strip->ClearTo(RgbColor(0,0,0));
        return;
    }

    unsigned long currentTimeMs = millis();
    if (currentTimeMs - _lastFrameTimeMs >= _frameDurationMs) {
        _lastFrameTimeMs = currentTimeMs;
        _currentFrameIndex = (_currentFrameIndex + 1) % _currentAnimation->frameCount;
    }

    // Render current frame
    // Each frame is FRAME_WIDTH x FRAME_HEIGHT pixels, each pixel 3 bytes (RGB)
    // Data is const uint8_t* frames[];
    const uint8_t* frameData = _currentAnimation->frames[_currentFrameIndex];

    for (uint8_t y = 0; y < FRAME_HEIGHT; ++y) {
        for (uint8_t x = 0; x < FRAME_WIDTH; ++x) {
            // Calculate index in the frameData array
            // Assuming row-major order for frame data: (y * FRAME_WIDTH + x) * 3 bytes
            uint16_t pixelDataOffset = (y * FRAME_WIDTH + x) * 3;

            uint8_t r = frameData[pixelDataOffset];
            uint8_t g = frameData[pixelDataOffset + 1];
            uint8_t b = frameData[pixelDataOffset + 2];

            int ledIndex = mapCoordinatesToIndex(x, y);

            if (ledIndex != -1 && ledIndex < _numLeds) {
                _strip->SetPixelColor(ledIndex, RgbColor(r, g, b));
            }
        }
    }
}
