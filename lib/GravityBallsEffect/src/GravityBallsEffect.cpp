#include "GravityBallsEffect.h"

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
    .ballColorSaturation = 1.0f
};

const GravityBallsEffect::Parameters GravityBallsEffect::PlasmaPreset = {
    .numBalls = 30, // 更多的球
    .gravityScale = 40.0f, // 对重力更敏感
    .dampingFactor = 0.98f, // 阻尼更小，更"滑溜"
    .sensorDeadZone = 1.0f,
    .restitution = 0.4f, // 弹性更小，像粘稠的等离子体
    .baseBrightness = 120,
    .brightnessCyclePeriodS = 5.0f,
    .minBrightnessScale = 0.4f,
    .maxBrightnessScale = 1.0f,
    .colorCyclePeriodS = 15.0f, // 颜色变化更慢
    .ballColorSaturation = 0.8f
};


GravityBallsEffect::GravityBallsEffect() {
    _balls = nullptr;
    _accel = nullptr;
    _strip = nullptr;
    setParameters(BouncyPreset); // 构造时加载默认预设
}

GravityBallsEffect::~GravityBallsEffect() {
    if (_balls != nullptr) delete[] _balls;
    if (_accel != nullptr) delete _accel;
}

void GravityBallsEffect::setParameters(const Parameters& params) {
    bool numBallsChanged = (_params.numBalls != params.numBalls);
    _params = params;
    // 如果小球数量改变，需要重新分配内存并初始化
    if (numBallsChanged && _strip != nullptr) {
        initBalls();
    }
}

void GravityBallsEffect::initBalls() {
    if (_balls != nullptr) {
        delete[] _balls;
    }
    _balls = new Ball[_params.numBalls];
    
    float ballRadius = 0.5f;
    float minSeparationDistSq = (2 * ballRadius) * (2 * ballRadius);

    for (int i = 0; i < _params.numBalls; ++i) {
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
        _balls[i].brightnessFactor = _params.minBrightnessScale;
        _balls[i].huePhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        _balls[i].hue = fmod(((0.0f / 1000.0f * 2.0f * PI / _params.colorCyclePeriodS) + _balls[i].huePhaseOffset) / (2.0f * PI), 1.0f);
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

    float ax_eff = (abs(rawAx) < _params.sensorDeadZone) ? 0 : rawAx;
    float az_eff = (abs(rawAz) < _params.sensorDeadZone) ? 0 : rawAz;

    float forceX = -ax_eff * _params.gravityScale;
    float forceY = az_eff * _params.gravityScale;

    // Update balls using parameters from _params
    for (int i = 0; i < _params.numBalls; ++i) {
        float sinWaveBright = (sin((2.0f * PI / _params.brightnessCyclePeriodS * totalTimeSeconds) + _balls[i].brightnessPhaseOffset) + 1.0f) / 2.0f;
        _balls[i].brightnessFactor = _params.minBrightnessScale + sinWaveBright * (_params.maxBrightnessScale - _params.minBrightnessScale);
        float rawHueAngle = (2.0f * PI / _params.colorCyclePeriodS * totalTimeSeconds) + _balls[i].huePhaseOffset;
        _balls[i].hue = fmod(rawHueAngle / (2.0f * PI), 1.0f);
        if (_balls[i].hue < 0.0f) _balls[i].hue += 1.0f;

        _balls[i].vx += forceX * dt;
        _balls[i].vy += forceY * dt;
        _balls[i].vx *= _params.dampingFactor;
        _balls[i].vy *= _params.dampingFactor;
        _balls[i].x += _balls[i].vx * dt;
        _balls[i].y += _balls[i].vy * dt;

        float ballRadius = 0.5f;
        if (_balls[i].x < ballRadius) { _balls[i].x = ballRadius; _balls[i].vx *= -_params.restitution; }
        else if (_balls[i].x > _matrixWidth - ballRadius) { _balls[i].x = _matrixWidth - ballRadius; _balls[i].vx *= -_params.restitution; }
        if (_balls[i].y < ballRadius) { _balls[i].y = ballRadius; _balls[i].vy *= -_params.restitution; }
        else if (_balls[i].y > _matrixHeight - ballRadius) { _balls[i].y = _matrixHeight - ballRadius; _balls[i].vy *= -_params.restitution; }
    }

    float ballRadius = 0.5f;
    float minSeparationDistSq = (2 * ballRadius) * (2 * ballRadius);
    float invBallMass = 1.0f;
    for (int i = 0; i < _params.numBalls; ++i) {
        for (int j = i + 1; j < _params.numBalls; ++j) {
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
                    float impulseMagnitude = -(1.0f + _params.restitution) * velAlongNormal / (2.0f * invBallMass);
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
    for (int i = 0; i < _params.numBalls; ++i) {
        int pixelX = round(_balls[i].x - ballRadius);
        int pixelY = round(_balls[i].y - ballRadius);
        pixelX = constrain(pixelX, 0, _matrixWidth - 1);
        pixelY = constrain(pixelY, 0, _matrixHeight - 1);
        int ledIndex = pixelY * _matrixWidth + (_matrixWidth - 1 - pixelX);
        if (ledIndex >= 0 && ledIndex < _numLeds) {
            HsbColor hsbColor(_balls[i].hue, _params.ballColorSaturation, _balls[i].brightnessFactor * (_params.baseBrightness / 255.0f));
            _strip->SetPixelColor(ledIndex, hsbColor);
        }
    }
}