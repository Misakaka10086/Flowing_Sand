#include "RippleEffect.h"

RippleEffect::RippleEffect() {
    _ripples = nullptr;
    _strip = nullptr;
    _maxRadius = 8 * 1.2f; 
}

RippleEffect::~RippleEffect() {
    if (_ripples != nullptr) {
        delete[] _ripples;
    }
}

void RippleEffect::Update() {
    if (_strip == nullptr || _ripples == nullptr) return;

    unsigned long currentTimeMs = millis();

    // Auto-create ripples
    if (currentTimeMs - _lastAutoRippleTimeMs >= (unsigned long)(_spawnIntervalS * 1000.0f)) {
        _lastAutoRippleTimeMs = currentTimeMs;

        float originX = _matrixWidth / 2.0f;
        float originY = _matrixHeight / 2.0f;

        if (_randomOrigin) {
            originX += random(-100, 101) / 100.0f;
            originY += random(-100, 101) / 100.0f;
            originX = constrain(originX, 0.5f, _matrixWidth - 0.5f);
            originY = constrain(originY, 0.5f, _matrixHeight - 0.5f);
        }

        float randomHue = random(0, 1000) / 1000.0f;
        
        _ripples[_nextRippleIndex].isActive = true;
        _ripples[_nextRippleIndex].originX = originX;
        _ripples[_nextRippleIndex].originY = originY;
        _ripples[_nextRippleIndex].startTimeMs = currentTimeMs;
        _ripples[_nextRippleIndex].hue = randomHue;
        _nextRippleIndex = (_nextRippleIndex + 1) % _maxRipples;
    }

    // Render LED matrix
    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int j_pixel = 0; j_pixel < _matrixHeight; ++j_pixel) {
        for (int i_pixel = 0; i_pixel < _matrixWidth; ++i_pixel) {
            float pixelCenterX = i_pixel + 0.5f;
            float pixelCenterY = j_pixel + 0.5f;

            float maxIntensityForThisPixel = 0.0f;
            HsbColor colorForThisPixel(0, 0, 0);

            for (int r_idx = 0; r_idx < _maxRipples; ++r_idx) {
                if (_ripples[r_idx].isActive) {
                    unsigned long rippleStartTime = _ripples[r_idx].startTimeMs;
                    float elapsedTimeS = (float)(currentTimeMs - rippleStartTime) / 1000.0f;
                    float currentRadius = elapsedTimeS * _speed;

                    if (currentRadius > _maxRadius + _thickness / 2.0f) {
                        _ripples[r_idx].isActive = false;
                        continue;
                    }

                    float distToOrigin = sqrt(pow(pixelCenterX - _ripples[r_idx].originX, 2) + pow(pixelCenterY - _ripples[r_idx].originY, 2));
                    float distToRippleEdge = abs(distToOrigin - currentRadius);

                    if (distToRippleEdge < _thickness / 2.0f) {
                        float intensityFactor = cos((distToRippleEdge / (_thickness / 2.0f)) * (PI / 2.0f));
                        intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);
                        if (intensityFactor > maxIntensityForThisPixel) {
                            maxIntensityForThisPixel = intensityFactor;
                            
                            // ***** 应用全局亮度乘数 *****
                            float finalBrightness = maxIntensityForThisPixel * _brightness;
                            
                            colorForThisPixel = HsbColor(_ripples[r_idx].hue, _saturation, finalBrightness);
                        }
                    }
                }
            }

            if (maxIntensityForThisPixel > 0.0f) {
                int ledIndex = j_pixel * _matrixWidth + (_matrixWidth - 1 - i_pixel);
                if (ledIndex >= 0 && ledIndex < _numLeds) {
                    _strip->SetPixelColor(ledIndex, colorForThisPixel);
                }
            }
        }
    }
}

// --- 公共配置接口实现 ---

void RippleEffect::setMaxRipples(uint8_t count) {
    if (count == _maxRipples) return;
    _maxRipples = count;
    if (_ripples != nullptr) delete[] _ripples;
    _ripples = new Ripple[_maxRipples];
    for (uint8_t i = 0; i < _maxRipples; ++i) {
        _ripples[i].isActive = false;
    }
    _nextRippleIndex = 0;
}
void RippleEffect::setSpeed(float speed) { _speed = speed; }
void RippleEffect::setThickness(float thickness) { _thickness = thickness; }
void RippleEffect::setSpawnInterval(float seconds) { _spawnIntervalS = seconds; }
void RippleEffect::setMaxRadius(float radius) { _maxRadius = radius; }
void RippleEffect::setRandomOrigin(bool random) { _randomOrigin = random; }
void RippleEffect::setSaturation(float saturation) { _saturation = constrain(saturation, 0.0f, 1.0f); }
void RippleEffect::setBrightness(float brightness) { // ***** 新增接口的实现 *****
    _brightness = constrain(brightness, 0.0f, 1.0f);
}