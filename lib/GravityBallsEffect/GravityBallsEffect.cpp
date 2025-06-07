#include "GravityBallsEffect.h"

GravityBallsEffect::GravityBallsEffect() {
    _balls = nullptr;
    _accel = nullptr;
    _strip = nullptr;
}

GravityBallsEffect::~GravityBallsEffect() {
    if (_balls != nullptr) delete[] _balls;
    if (_accel != nullptr) delete _accel;
}

void GravityBallsEffect::setNumberOfBalls(uint8_t num) {
    _numBalls = num;
    if (_balls != nullptr) {
        delete[] _balls;
    }
    _balls = new Ball[_numBalls];
    initBalls();
}

void GravityBallsEffect::initBalls() {
    if (_balls == nullptr || _numBalls == 0) return;

    float minSeparationDistSq = (2 * _ballRadius) * (2 * _ballRadius);

    for (int i = 0; i < _numBalls; ++i) {
        bool positionOK;
        do {
            positionOK = true;
            _balls[i].x = random(_matrixWidth * 100) / 100.0f;
            _balls[i].y = random(_matrixHeight * 100) / 100.0f;
            for (int j = 0; j < i; ++j) {
                float dx_init = _balls[i].x - _balls[j].x;
                float dy_init = _balls[i].y - _balls[j].y;
                if (dx_init * dx_init + dy_init * dy_init < minSeparationDistSq) {
                    positionOK = false;
                    break;
                }
            }
        } while (!positionOK);

        _balls[i].vx = 0;
        _balls[i].vy = 0;
        _balls[i].brightnessPhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        _balls[i].brightnessFactor = _minBrightnessScale;
        _balls[i].huePhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        _balls[i].hue = fmod(((0.0f / 1000.0f * 2.0f * PI / _colorCyclePeriodS) + _balls[i].huePhaseOffset) / (2.0f * PI), 1.0f);
        if (_balls[i].hue < 0) _balls[i].hue += 1.0f;
    }
}


void GravityBallsEffect::Update() {
    if (_strip == nullptr || _accel == nullptr || _balls == nullptr) return;

    unsigned long currentTime = millis();
    float dt = (currentTime - _lastUpdateTime) / 1000.0f;
    if (dt <= 0.0001f) dt = 0.001f;
    _lastUpdateTime = currentTime;
    float totalTimeSeconds = currentTime / 1000.0f;

    sensors_event_t event;
    _accel->getEvent(&event);
    float rawAx = event.acceleration.x;
    float rawAz = event.acceleration.z;

    float ax_eff = (abs(rawAx) < _sensorDeadZone) ? 0 : rawAx;
    float az_eff = (abs(rawAz) < _sensorDeadZone) ? 0 : rawAz;

    float forceX = -ax_eff * _gravityScale;
    float forceY = az_eff * _gravityScale;

    // Update balls
    for (int i = 0; i < _numBalls; ++i) {
        // Update brightness and color
        float sinWaveBright = (sin((2.0f * PI / _brightnessCyclePeriodS * totalTimeSeconds) + _balls[i].brightnessPhaseOffset) + 1.0f) / 2.0f;
        _balls[i].brightnessFactor = _minBrightnessScale + sinWaveBright * (_maxBrightnessScale - _minBrightnessScale);
        float rawHueAngle = (2.0f * PI / _colorCyclePeriodS * totalTimeSeconds) + _balls[i].huePhaseOffset;
        _balls[i].hue = fmod(rawHueAngle / (2.0f * PI), 1.0f);
        if (_balls[i].hue < 0.0f) _balls[i].hue += 1.0f;

        // Update physics
        _balls[i].vx += forceX * dt;
        _balls[i].vy += forceY * dt;
        _balls[i].vx *= _dampingFactor;
        _balls[i].vy *= _dampingFactor;
        _balls[i].x += _balls[i].vx * dt;
        _balls[i].y += _balls[i].vy * dt;

        // Boundary collisions
        if (_balls[i].x < _ballRadius) { _balls[i].x = _ballRadius; _balls[i].vx *= -_restitution; }
        else if (_balls[i].x > _matrixWidth - _ballRadius) { _balls[i].x = _matrixWidth - _ballRadius; _balls[i].vx *= -_restitution; }
        if (_balls[i].y < _ballRadius) { _balls[i].y = _ballRadius; _balls[i].vy *= -_restitution; }
        else if (_balls[i].y > _matrixHeight - _ballRadius) { _balls[i].y = _matrixHeight - _ballRadius; _balls[i].vy *= -_restitution; }
    }

    // Ball-to-ball collisions
    float minSeparationDistSq = (2 * _ballRadius) * (2 * _ballRadius);
    float invBallMass = 1.0f; // Assuming mass = 1
    for (int i = 0; i < _numBalls; ++i) {
        for (int j = i + 1; j < _numBalls; ++j) {
            float dx = _balls[j].x - _balls[i].x;
            float dy = _balls[j].y - _balls[i].y;
            float distSq = dx * dx + dy * dy;
            if (distSq < minSeparationDistSq && distSq > 0.00001f) {
                float dist = sqrt(distSq);
                float nx = dx / dist;
                float ny = dy / dist;
                float rvx = _balls[j].vx - _balls[i].vx;
                float rvy = _balls[j].vy - _balls[i].vy;
                float velAlongNormal = rvx * nx + rvy * ny;
                if (velAlongNormal < 0) {
                    float impulseMagnitude = -(1.0f + _restitution) * velAlongNormal / (2.0f * invBallMass);
                    _balls[i].vx -= impulseMagnitude * nx * invBallMass;
                    _balls[i].vy -= impulseMagnitude * ny * invBallMass;
                    _balls[j].vx += impulseMagnitude * nx * invBallMass;
                    _balls[j].vy += impulseMagnitude * ny * invBallMass;
                }
                float overlap = (2 * _ballRadius) - dist;
                float correction_factor = 0.5f;
                _balls[i].x -= nx * overlap * correction_factor;
                _balls[i].y -= ny * overlap * correction_factor;
                _balls[j].x += nx * overlap * correction_factor;
                _balls[j].y += ny * overlap * correction_factor;
            }
        }
    }

    // Render balls
    _strip->ClearTo(RgbColor(0, 0, 0));
    for (int i = 0; i < _numBalls; ++i) {
        int pixelX = round(_balls[i].x - _ballRadius);
        int pixelY = round(_balls[i].y - _ballRadius);
        pixelX = constrain(pixelX, 0, _matrixWidth - 1);
        pixelY = constrain(pixelY, 0, _matrixHeight - 1);
        int ledIndex = pixelY * _matrixWidth + (_matrixWidth - 1 - pixelX);
        if (ledIndex >= 0 && ledIndex < _numLeds) {
            HsbColor hsbColor(_balls[i].hue, _ballColorSaturation, _balls[i].brightnessFactor * (_baseBrightness / 255.0f));
            _strip->SetPixelColor(ledIndex, hsbColor);
        }
    }
}

// --- 公共配置接口实现 ---
void GravityBallsEffect::setGravityScale(float scale) { _gravityScale = scale; }
void GravityBallsEffect::setDampingFactor(float damping) { _dampingFactor = constrain(damping, 0.0f, 1.0f); }
void GravityBallsEffect::setSensorDeadZone(float deadzone) { _sensorDeadZone = deadzone; }
void GravityBallsEffect::setRestitution(float restitution) { _restitution = constrain(restitution, 0.0f, 1.0f); }
void GravityBallsEffect::setBaseBrightness(uint8_t brightness) { _baseBrightness = brightness; }
void GravityBallsEffect::setBrightnessCyclePeriod(float seconds) { _brightnessCyclePeriodS = max(0.1f, seconds); }
void GravityBallsEffect::setColorCyclePeriod(float seconds) { _colorCyclePeriodS = max(0.1f, seconds); }
void GravityBallsEffect::setBallColorSaturation(float saturation) { _ballColorSaturation = constrain(saturation, 0.0f, 1.0f); }