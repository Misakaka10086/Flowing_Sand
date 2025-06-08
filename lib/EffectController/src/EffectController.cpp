#include "EffectController.h"
#include <ArduinoJson.h>

void EffectController::Update() {
    if (!_strip) return;

    switch (_currentEffect) {
        case EffectType::GRAVITY_BALLS: gravityEffect.Update(); break;
        case EffectType::ZEN_LIGHTS:    zenEffect.Update();    break;
        case EffectType::CODE_RAIN:     codeRainEffect.Update(); break;
        case EffectType::RIPPLE:        rippleEffect.Update();   break;
        case EffectType::SCROLLING_TEXT: scrollingTextEffect.Update(); break;
        case EffectType::LAVA_LAMP:     lavaLampEffect.Update(); break;
    }
}

void EffectController::processCommand(const char* jsonCommand) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonCommand);

    if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return;
    }

    // 获取 `effect` 字段，准备分发
    const char* effectName = doc["effect"];

    // 如果 `effect` 字段存在，就切换效果
    if (effectName) {
        if (strcmp(effectName, "gravity_balls") == 0) _currentEffect = EffectType::GRAVITY_BALLS;
        else if (strcmp(effectName, "zen_lights") == 0) _currentEffect = EffectType::ZEN_LIGHTS;
        else if (strcmp(effectName, "code_rain") == 0) _currentEffect = EffectType::CODE_RAIN;
        else if (strcmp(effectName, "ripple") == 0) _currentEffect = EffectType::RIPPLE;
        else if (strcmp(effectName, "scrolling_text") == 0) _currentEffect = EffectType::SCROLLING_TEXT;
        else if (strcmp(effectName, "lava_lamp") == 0) _currentEffect = EffectType::LAVA_LAMP;
        Serial.printf("Switched to effect: %s\n", effectName);
    }

    // 处理预设切换
    const char* presetName = doc["prePara"];
    if (presetName) {
        switch (_currentEffect) {
            case EffectType::CODE_RAIN:
                codeRainEffect.setPreset(presetName);
                break;
            case EffectType::GRAVITY_BALLS:
                gravityEffect.setPreset(presetName);
                break;
            case EffectType::RIPPLE:
                rippleEffect.setPreset(presetName);
                break;
            case EffectType::ZEN_LIGHTS:
                zenEffect.setPreset(presetName);
                break;
            case EffectType::SCROLLING_TEXT:
                scrollingTextEffect.setPreset(presetName);
            case EffectType::LAVA_LAMP:
                lavaLampEffect.setPreset(presetName);
                break;
            // 其他效果的预设切换可以在这里添加
            default:
                Serial.println("Preset switching not supported for current effect");
                break;
        }
    }

    // 如果 `params` 字段存在，就将其传递给当前的效果类
    if (doc["params"].is<JsonObject>()) {
        JsonObject paramsObj = doc["params"].as<JsonObject>();
        // 将 params JSON 对象序列化回字符串，然后传递
        String paramsStr;
        serializeJson(paramsObj, paramsStr);
        
        Serial.printf("Dispatching params to current effect: %s\n", paramsStr.c_str());

        switch (_currentEffect) {
            case EffectType::GRAVITY_BALLS:
                gravityEffect.setParameters(paramsStr.c_str());
                break;
            case EffectType::ZEN_LIGHTS:
                zenEffect.setParameters(paramsStr.c_str());
                break;
            case EffectType::CODE_RAIN:
                codeRainEffect.setParameters(paramsStr.c_str()); 
                break;
            case EffectType::RIPPLE:
                rippleEffect.setParameters(paramsStr.c_str());
                break;
            case EffectType::SCROLLING_TEXT:
                scrollingTextEffect.setParameters(paramsStr.c_str());
            case EffectType::LAVA_LAMP:
                lavaLampEffect.setParameters(paramsStr.c_str());
                break;
        }
    }
}