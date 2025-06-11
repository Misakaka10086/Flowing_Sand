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
        float baseBrightness; // Overall brightness (0.0 to 1.0)
    };

    // Define any presets if needed, e.g.:
    static const Parameters DefaultPreset; // Declaration

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
};

// Template-based Begin must be in the header file
template<typename T_NeoPixelBus>
void AnimationEffect::Begin(T_NeoPixelBus& strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;

    // Initialize with default parameters or a specific preset
    // The initial version called setParameters(DefaultPreset) here.
    // The constructor in the original .cpp also set _activeParams = DefaultPreset.
    // To match the first submission's likely state, we'll assume DefaultPreset is applied here.
    // However, the .cpp will define DefaultPreset.
    // The constructor of AnimationEffect (in the original .cpp) should handle _activeParams = DefaultPreset;
    // _currentAnimation = findAnimationByName(_activeParams.prePara);
    // _frameDurationMs = 1000.0f / _activeParams.baseSpeed;
    // Let's keep Begin simple as per the very first version of this file.
    // The actual application of DefaultPreset was primarily in the constructor in the first .cpp.

    _lastFrameTimeMs = millis();
}

#endif // ANIMATION_EFFECT_H
