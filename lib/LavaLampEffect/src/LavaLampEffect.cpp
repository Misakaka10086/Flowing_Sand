#include "LavaLampEffect.h"
#include <ArduinoJson.h>
#include <math.h>

const LavaLampEffect::Parameters LavaLampEffect::ClassicLavaPreset = {
    .numBlobs = 4,
    .threshold = 1.0f,
    .baseSpeed = 0.8f,
    .baseBrightness = 0.1f,
    .baseHue = 0.0f,   // 红色
    .hueRange = 0.16f, // 渐变到黄色
    .prePara = "ClassicLava"};

const LavaLampEffect::Parameters LavaLampEffect::MercuryPreset = {
    .numBlobs = 5,
    .threshold = 1.2f,
    .baseSpeed = 1.2f,
    .baseBrightness = 0.1f,
    .baseHue = 0.9f, // 使用饱和度为0来模拟银色
    .hueRange = 0.0f,
    .prePara = "Mercury"};

LavaLampEffect::LavaLampEffect()
{
    _blobs = nullptr;
    setParameters(ClassicLavaPreset);
}

LavaLampEffect::~LavaLampEffect()
{
    if (_blobs)
        delete[] _blobs;
}

void LavaLampEffect::setParameters(const Parameters &params)
{
    bool numBlobsChanged = (_blobs == nullptr) || (_params.numBlobs != params.numBlobs);
    _params = params;

    if (numBlobsChanged && _strip != nullptr)
    {
        if (_blobs)
            delete[] _blobs;
        _blobs = new Metaball[_params.numBlobs];
        initBlobs();
    }
}

void LavaLampEffect::setParameters(const char *jsonParams)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);
    if (error)
    {
        Serial.println("LavaLampEffect::setParameters failed to parse JSON: " + String(error.c_str()));
        return;
    }

    // 复制当前参数，以便我们可以安全地修改它
    Parameters newParams = _params;

    // 检查JSON中是否有numBlobs，因为它需要特殊处理（内存重新分配）
    if (doc["numBlobs"].is<uint8_t>())
    {
        newParams.numBlobs = doc["numBlobs"].as<uint8_t>();
    }

    // 使用 | 操作符来获取JSON中的值，如果不存在则保留原值
    newParams.threshold = doc["threshold"] | newParams.threshold;
    newParams.baseSpeed = doc["baseSpeed"] | newParams.baseSpeed;
    newParams.baseBrightness = doc["baseBrightness"] | newParams.baseBrightness;
    newParams.baseHue = doc["baseHue"] | newParams.baseHue;
    newParams.hueRange = doc["hueRange"] | newParams.hueRange;

    // 检查 prePara 字段，以确定是否需要更新预设名称
    if (doc["prePara"].is<const char *>())
    {
        const char *newPrePara = doc["prePara"];
        if (strcmp(newPrePara, "ClassicLava") == 0)
        {
            newParams.prePara = ClassicLavaPreset.prePara;
        }
        else if (strcmp(newPrePara, "Mercury") == 0)
        {
            newParams.prePara = MercuryPreset.prePara;
        }
    }

    // 最后，调用结构体版本的 setParameters。
    // 这个函数会处理 numBlobs 改变时内存的重新分配。
    setParameters(newParams);

    Serial.println("LavaLampEffect parameters updated via JSON.");
}

void LavaLampEffect::setPreset(const char *presetName)
{
    if (strcmp(presetName, "next") == 0)
    {
        // 使用 prePara 字段来判断当前预设并切换到下一个
        if (strcmp(_params.prePara, "ClassicLava") == 0)
        {
            setParameters(MercuryPreset);
            Serial.println("Switched to MercuryPreset");
        }
        else
        {
            // 从 Mercury (或其他) 切换回 ClassicLava
            setParameters(ClassicLavaPreset);
            Serial.println("Switched to ClassicLavaPreset");
        }
    }
    else if (strcmp(presetName, "ClassicLava") == 0)
    {
        setParameters(ClassicLavaPreset);
        Serial.println("Switched to ClassicLavaPreset");
    }
    else if (strcmp(presetName, "Mercury") == 0)
    {
        setParameters(MercuryPreset);
        Serial.println("Switched to MercuryPreset");
    }
    else
    {
        Serial.println("Unknown preset name for LavaLampEffect: " + String(presetName));
    }
}

void LavaLampEffect::initBlobs()
{
    for (int i = 0; i < _params.numBlobs; ++i)
    {
        _blobs[i].x = random(0, _matrixWidth * 100) / 100.0f;
        _blobs[i].y = random(0, _matrixHeight * 100) / 100.0f;
        _blobs[i].vx = (random(0, 200) - 100) / 100.0f * _params.baseSpeed;
        _blobs[i].vy = (random(0, 200) - 100) / 100.0f * _params.baseSpeed;
        _blobs[i].radius = random(150, 250) / 100.0f; // 半径在 1.5 到 2.5 之间
    }
}

void LavaLampEffect::Update()
{
    if (!_strip || !_blobs)
        return;

    unsigned long currentTime = millis();
    float deltaTime = (currentTime - _lastUpdateTime) / 1000.0f;
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;
    _lastUpdateTime = currentTime;

    // 1. 移动所有Metaballs
    for (int i = 0; i < _params.numBlobs; ++i)
    {
        Metaball &blob = _blobs[i];
        blob.x += blob.vx * deltaTime * _params.baseSpeed;
        blob.y += blob.vy * deltaTime * _params.baseSpeed;

        // 边界碰撞反弹
        if (blob.x < 0 || blob.x > _matrixWidth - 1)
            blob.vx *= -1;
        if (blob.y < 0 || blob.y > _matrixHeight - 1)
            blob.vy *= -1;
    }

    _strip->ClearTo(RgbColor(0));

    // 2. 为每个像素计算能量并渲染
    for (int py = 0; py < _matrixHeight; ++py)
    {
        for (int px = 0; px < _matrixWidth; ++px)
        {
            float totalEnergy = 0.0f;
            float pixelCenterX = px + 0.5f;
            float pixelCenterY = py + 0.5f;

            // 累加所有Metaballs的能量
            for (int i = 0; i < _params.numBlobs; ++i)
            {
                Metaball &blob = _blobs[i];
                float dx = pixelCenterX - blob.x;
                float dy = pixelCenterY - blob.y;
                float distSq = dx * dx + dy * dy;
                if (distSq == 0.0f)
                    distSq = 0.0001f; // 避免除以零

                // 核心公式: E = r^2 / d^2
                totalEnergy += (blob.radius * blob.radius) / distSq;
            }

            // 3. 如果能量超过阈值，则点亮像素
            if (totalEnergy > _params.threshold)
            {
                // 计算颜色
                float excessEnergy = totalEnergy - _params.threshold;
                float brightness = constrain(excessEnergy * 0.5f, 0.0f, 1.0f) * _params.baseBrightness;
                float hue = _params.baseHue + constrain(excessEnergy * 0.2f, 0.0f, 1.0f) * _params.hueRange;

                float saturation = 1.0f;
                // 特殊处理水银效果
                if (strcmp(_params.prePara, "Mercury") == 0)
                {
                    saturation = 0.0f; // 银色就是无饱和度的白色
                }

                HsbColor color(hue, saturation, brightness);

                // 使用你确认的坐标系映射
                int led_index = (7 - py) * 8 + (7 - px);
                _strip->SetPixelColor(led_index, color);
            }
        }
    }
}