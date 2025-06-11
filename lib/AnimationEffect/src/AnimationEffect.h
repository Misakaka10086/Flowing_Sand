#ifndef ANIMATION_EFFECT_H
#define ANIMATION_EFFECT_H

#include <NeoPixelBus.h>
#include "../../../include/MyAnimationData.h" // To access Animation struct and data
#include "../../../include/TransitionUtils.h" // For transitions

class AnimationEffect {
public:
    struct Parameters {
        const char* prePara; // Name of the animation to play (e.g., "animated_heart_anim")
        float baseSpeed;     // Playback speed (frames per second)
        // Add other parameters here if needed in the future, e.g., brightness
    };

    // Define any presets if needed, e.g.:
    // static const Parameters DefaultPreset;

    AnimationEffect();
    ~AnimationEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight);

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams); // For MQTT control
    void setPreset(const char* presetName);     // For MQTT control

private:
    // Helper to map 2D coordinates to 1D LED strip index
    int mapCoordinatesToIndex(int x, int y);

    // Pointer to the LED strip
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 0;
    uint8_t _matrixHeight = 0;

    // Animation playback state
    const Animation* _currentAnimation = nullptr; // Pointer to the active animation data
    uint16_t _currentFrameIndex = 0;
    unsigned long _lastFrameTimeMs = 0;
    float _frameDurationMs = 100.0f; // Calculated from baseSpeed

    // Parameters
    Parameters _activeParams;
    Parameters _targetParams;
    Parameters _oldParams;

    bool _effectInTransition;
    unsigned long _effectTransitionStartTimeMs;
    unsigned long _effectTransitionDurationMs;

    // Add a default animation preset if makes sense, e.g. using animated_heart_anim
    static const Parameters DefaultPreset;
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void AnimationEffect::Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;

    // Initialize with default parameters or a specific preset
    // For now, let's create a default preset that will be defined in the .cpp
    // setParameters(DefaultPreset); // This will be uncommented once DefaultPreset is defined

    _lastFrameTimeMs = millis();
}

#endif // ANIMATION_EFFECT_H
