#ifndef EFFECT_CONTROLLER_H
#define EFFECT_CONTROLLER_H

#include <NeoPixelBus.h>
#include "ZenLightsEffect.h"
#include "GravityBallsEffect.h"
#include "CodeRainEffect.h"
#include "RippleEffect.h"
#include "ScrollingTextEffect.h"
#include "LavaLampEffect.h"
#include "../../../include/DebugUtils.h"

// 使用枚举来表示效果，更安全
enum class EffectType {
    GRAVITY_BALLS,
    ZEN_LIGHTS,
    CODE_RAIN,
    RIPPLE,
    SCROLLING_TEXT,
    LAVA_LAMP
};

class EffectController
{
public:
    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip) {
        const uint8_t MATRIX_WIDTH = 16;
        const uint8_t MATRIX_HEIGHT = 16;
        _strip = &strip;
        // 初始化所有效果
        zenEffect.Begin(strip, MATRIX_WIDTH, MATRIX_HEIGHT);
        if (!gravityEffect.Begin(strip, MATRIX_WIDTH, MATRIX_HEIGHT)) {
            DEBUG_PRINTLN("Gravity Effect FAILED to init!");
        }
        codeRainEffect.Begin(strip, MATRIX_WIDTH, MATRIX_HEIGHT);
        rippleEffect.Begin(strip, MATRIX_WIDTH, MATRIX_HEIGHT);
        scrollingTextEffect.Begin(strip);
        lavaLampEffect.Begin(strip, MATRIX_WIDTH, MATRIX_HEIGHT);
    }
    
    // 主更新循环
    void Update();

    // 从MQTT接收JSON指令
    void processCommand(const char* jsonCommand);

private:
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;

    ZenLightsEffect zenEffect;
    GravityBallsEffect gravityEffect;
    CodeRainEffect codeRainEffect;
    RippleEffect rippleEffect;
    ScrollingTextEffect scrollingTextEffect;
    LavaLampEffect lavaLampEffect;
    
    EffectType _currentEffect = EffectType::GRAVITY_BALLS; // 默认效果
};

#endif // EFFECT_CONTROLLER_H