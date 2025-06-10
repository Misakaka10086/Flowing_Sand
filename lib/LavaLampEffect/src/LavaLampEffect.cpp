#include "LavaLampEffect.h"
#include <ArduinoJson.h>
#include <math.h>

const LavaLampEffect::Parameters LavaLampEffect::ClassicLavaPreset = {
    .numBlobs = 4,
    .threshold = 1.0f,
    .baseSpeed = 0.8f,
    .baseBrightness = 0.1f,
    .baseColor = "#FF0000", // 红色
    .hueRange = 0.16f,      // 渐变到黄色
    .prePara = "ClassicLava"};

const LavaLampEffect::Parameters LavaLampEffect::MercuryPreset = {
    .numBlobs = 5,
    .threshold = 1.2f,
    .baseSpeed = 1.2f,
    .baseBrightness = 0.1f,
    .baseColor = "#FFFFFF", // 银色/白色
    .hueRange = 0.0f,
    .prePara = "Mercury"};

// 辅助函数实现
void LavaLampEffect::hexToRgb(const char* hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hex == nullptr) return;
    // 如果有'#'，跳过它
    if (hex[0] == '#') {
        hex++;
    }
    // 将16进制字符串转换为长整型
    long colorValue = strtol(hex, NULL, 16);
    // 提取R, G, B分量
    r = (colorValue >> 16) & 0xFF;
    g = (colorValue >> 8) & 0xFF;
    b = colorValue & 0xFF;
}

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

    // 将 baseColor 16进制字符串转换为 HSB 并存储 H 值
    uint8_t r, g, b;
    hexToRgb(_params.baseColor, r, g, b);
    RgbColor rgb(r, g, b);
    HsbColor hsb(rgb);
    _internalBaseHue = hsb.H;

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

    Parameters newParams = _params;

    if (doc["numBlobs"].is<uint8_t>())
    {
        newParams.numBlobs = doc["numBlobs"].as<uint8_t>();
    }

    newParams.threshold = doc["threshold"] | newParams.threshold;
    newParams.baseSpeed = doc["baseSpeed"] | newParams.baseSpeed;
    newParams.baseBrightness = doc["baseBrightness"] | newParams.baseBrightness;
    // 从JSON中获取baseColor字符串
    newParams.baseColor = doc["baseColor"] | newParams.baseColor;
    newParams.hueRange = doc["hueRange"] | newParams.hueRange;

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

    // 调用结构体版本的 setParameters，它会处理所有转换和内存分配
    setParameters(newParams);

    Serial.println("LavaLampEffect parameters updated via JSON.");
}

void LavaLampEffect::setPreset(const char *presetName)
{
    if (strcmp(presetName, "next") == 0)
    {
        if (strcmp(_params.prePara, "ClassicLava") == 0)
        {
            setParameters(MercuryPreset);
            Serial.println("Switched to MercuryPreset");
        }
        else
        {
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

int LavaLampEffect::mapCoordinatesToIndex(int x, int y) {
    const int module_width = 8;
    const int module_height = 8;
    const int leds_per_module = module_width * module_height;
    int module_col = x / module_width;
    int module_row = y / module_height;
    int base_index;
    if (module_row == 1 && module_col == 1) base_index = 0;
    else if (module_row == 1 && module_col == 0) base_index = leds_per_module * 1;
    else if (module_row == 0 && module_col == 1) base_index = leds_per_module * 2;
    else base_index = leds_per_module * 3;
    int local_x = x % module_width;
    int local_y = y % module_height;
    int local_offset = (module_height - 1 - local_y) * module_width + (module_width - 1 - local_x);
    return base_index + local_offset;
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

    for (int i = 0; i < _params.numBlobs; ++i)
    {
        Metaball &blob = _blobs[i];
        blob.x += blob.vx * deltaTime * _params.baseSpeed;
        blob.y += blob.vy * deltaTime * _params.baseSpeed;

        if (blob.x < 0 || blob.x > _matrixWidth - 1)
            blob.vx *= -1;
        if (blob.y < 0 || blob.y > _matrixHeight - 1)
            blob.vy *= -1;
    }

    _strip->ClearTo(RgbColor(0));

    for (int py = 0; py < _matrixHeight; ++py)
    {
        for (int px = 0; px < _matrixWidth; ++px)
        {
            float totalEnergy = 0.0f;
            float pixelCenterX = px + 0.5f;
            float pixelCenterY = py + 0.5f;

            for (int i = 0; i < _params.numBlobs; ++i)
            {
                Metaball &blob = _blobs[i];
                float dx = pixelCenterX - blob.x;
                float dy = pixelCenterY - blob.y;
                float distSq = dx * dx + dy * dy;
                if (distSq == 0.0f)
                    distSq = 0.0001f;

                totalEnergy += (blob.radius * blob.radius) / distSq;
            }

            if (totalEnergy > _params.threshold)
            {
                float excessEnergy = totalEnergy - _params.threshold;
                float brightness = constrain(excessEnergy * 0.5f, 0.0f, 1.0f) * _params.baseBrightness;
                float hue = _internalBaseHue + constrain(excessEnergy * 0.2f, 0.0f, 1.0f) * _params.hueRange;

                float saturation = 1.0f;
                if (strcmp(_params.prePara, "Mercury") == 0)
                {
                    saturation = 0.0f; // 银色就是无饱和度的白色
                }

                HsbColor color(hue, saturation, brightness);

                int led_index = mapCoordinatesToIndex(px, py);
                if (led_index >= 0 && led_index < _numLeds) {
                    _strip->SetPixelColor(led_index, color);
                }
            }
        }
    }
}