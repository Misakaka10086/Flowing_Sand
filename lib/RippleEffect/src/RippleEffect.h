#ifndef RIPPLE_EFFECT_H
#define RIPPLE_EFFECT_H

#include "../../../include/TransitionUtils.h"
#include <NeoPixelBus.h>
#include <math.h>


class RippleEffect {
public:
  struct Parameters {
    uint8_t maxRipples;
    float speed;
    float acceleration;
    float thickness;
    float spawnIntervalS;
    float maxRadius;
    bool randomOrigin;
    float saturation;
    float baseBrightness;
    float sharpness; // ADDED: Controls the brightness falloff curve
    const char *prePara;
  };

  static const Parameters WaterDropPreset;
  static const Parameters EnergyPulsePreset;

  RippleEffect();
  ~RippleEffect();

  template <typename T_NeoPixelBus>
  void Begin(T_NeoPixelBus &strip, uint8_t matrixWidth, uint8_t matrixHeight) {
    _strip = &strip;
    _numLeds = strip.PixelCount();
    _matrixWidth = matrixWidth;
    _matrixHeight = matrixHeight;

    setParameters(WaterDropPreset);

    _lastAutoRippleTimeMs = millis();
  }

  void Update();
  void setParameters(const Parameters &params);
  void setParameters(const char *jsonParams);
  void setPreset(const char *presetName);

private:
  struct Ripple {
    bool isActive;
    float originX, originY;
    unsigned long startTimeMs;
    float hue;
  };

  int mapCoordinatesToIndex(int x, int y);

  NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> *_strip = nullptr;
  uint16_t _numLeds = 0;
  uint8_t _matrixWidth = 0;
  uint8_t _matrixHeight = 0;

  Ripple *_ripples = nullptr;
  uint8_t _nextRippleIndex = 0;
  unsigned long _lastAutoRippleTimeMs = 0;

  Parameters _activeParams;
  Parameters _targetParams;
  Parameters _oldParams;

  bool _effectInTransition;
  unsigned long _effectTransitionStartTimeMs;
  unsigned long _effectTransitionDurationMs;
};

#endif // RIPPLE_EFFECT_H