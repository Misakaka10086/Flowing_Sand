#include <NeoPixelBus.h>
#include <math.h>

// --- 调试开关 ---
// #define DEBUG_SERIAL

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// NeoPixelBus 对象
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);

// --- 代码雨参数 ---
const int NUM_STREAMS = MATRIX_WIDTH; 
const float MIN_SPEED = 12.0f;       
const float MAX_SPEED = 36.0f;       
const int MIN_STREAM_LENGTH = 3;    
const int MAX_STREAM_LENGTH = MATRIX_HEIGHT -1; 
const float SPAWN_PROBABILITY = 0.15f; 
const unsigned long MIN_SPAWN_COOLDOWN_MS = 50;  
const unsigned long MAX_SPAWN_COOLDOWN_MS = 200; 

// 颜色参数
const float BASE_HUE = 0.33f; 
const float HUE_VARIATION = 0.05f; 
const float SATURATION = 1.0f;
const uint8_t MAX_BRIGHTNESS_VALUE = 200; 
const uint8_t BASE_LED_BRIGHTNESS = 64;  

// ***** 重命名结构体以避免与 Arduino Stream 类冲突 *****
struct CodeStream { // <--- RENAMED FROM Stream
    bool isActive;
    float currentY; 
    float speed;
    int length;
    unsigned long spawnCooldownMs;
    unsigned long lastActivityTimeMs; 
    float hue; 
};

CodeStream codeStreams[NUM_STREAMS]; // <--- RENAMED ARRAY VARIABLE

// HSV to RGB 
RgbColor HsvToRgb(float h, float s, float v_component) { 
    float r_hsv, g_hsv, b_hsv;
    float v = v_component * (MAX_BRIGHTNESS_VALUE / 255.0f); 

    if (s == 0.0f) {
        r_hsv = g_hsv = b_hsv = v;
    } else {
        int i = floor(h * 6.0f);
        float f = (h * 6.0f) - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));
        i = i % 6;
        switch (i) {
            case 0: r_hsv = v; g_hsv = t; b_hsv = p; break;
            case 1: r_hsv = q; g_hsv = v; b_hsv = p; break;
            case 2: r_hsv = p; g_hsv = v; b_hsv = t; break;
            case 3: r_hsv = p; g_hsv = q; b_hsv = v; break;
            case 4: r_hsv = t; g_hsv = p; b_hsv = v; break;
            case 5: r_hsv = v; g_hsv = p; b_hsv = q; break;
            default: r_hsv = 0; g_hsv = 0; b_hsv = 0; break;
        }
    }
    return RgbColor((uint8_t)(r_hsv * BASE_LED_BRIGHTNESS),
                    (uint8_t)(g_hsv * BASE_LED_BRIGHTNESS),
                    (uint8_t)(b_hsv * BASE_LED_BRIGHTNESS));
}


void setup() {
#ifdef DEBUG_SERIAL
    Serial.begin(115200);
    while (!Serial) { delay(10); } 
    Serial.println("ESP32 Matrix Code Rain Effect (Fixed Naming)");
#endif

    strip.Begin();
    strip.Show(); 

    randomSeed(millis());

    for (int i = 0; i < NUM_STREAMS; ++i) {
        codeStreams[i].isActive = false; // <--- RENAMED
        codeStreams[i].lastActivityTimeMs = millis() - random(MIN_SPAWN_COOLDOWN_MS, MAX_SPAWN_COOLDOWN_MS); // <--- RENAMED
        codeStreams[i].spawnCooldownMs = random(MIN_SPAWN_COOLDOWN_MS, MAX_SPAWN_COOLDOWN_MS); // <--- RENAMED
    }

#ifdef DEBUG_SERIAL
    Serial.println("Setup complete.");
#endif
}

unsigned long lastFrameTimeMs = 0;
unsigned long loopCounter = 0;

void loop() {
    unsigned long currentTimeMs = millis();
    float deltaTimeS = (currentTimeMs - lastFrameTimeMs) / 1000.0f;
    if (deltaTimeS <= 0.001f) { 
        deltaTimeS = 0.001f;
    }
    lastFrameTimeMs = currentTimeMs;
    loopCounter++;

    strip.ClearTo(RgbColor(0, 0, 0));

    // 1. 更新和生成流
    for (int x = 0; x < NUM_STREAMS; ++x) {
        if (codeStreams[x].isActive) { // <--- RENAMED (and all subsequent uses of codeStreams[x].MEMBER)
            codeStreams[x].currentY += codeStreams[x].speed * deltaTimeS;

            if (codeStreams[x].currentY - codeStreams[x].length >= MATRIX_HEIGHT) {
                codeStreams[x].isActive = false;
                codeStreams[x].lastActivityTimeMs = currentTimeMs;
                codeStreams[x].spawnCooldownMs = random(MIN_SPAWN_COOLDOWN_MS, MAX_SPAWN_COOLDOWN_MS);
#ifdef DEBUG_SERIAL
                if (Serial && loopCounter % 20 == 0) Serial.printf("Stream %d deactivated.\n", x);
#endif
            }
        } else {
            if (currentTimeMs - codeStreams[x].lastActivityTimeMs >= codeStreams[x].spawnCooldownMs) {
                if (random(1000) / 1000.0f < SPAWN_PROBABILITY) {
                    codeStreams[x].isActive = true;
                    codeStreams[x].currentY = -random(0, MATRIX_HEIGHT*2); 
                    codeStreams[x].speed = random(MIN_SPEED * 100, MAX_SPEED * 100) / 100.0f;
                    codeStreams[x].length = random(MIN_STREAM_LENGTH, MAX_STREAM_LENGTH + 1);
                    codeStreams[x].hue = BASE_HUE + (random(-HUE_VARIATION * 100, HUE_VARIATION * 100) / 100.0f);
                    if (codeStreams[x].hue < 0.0f) codeStreams[x].hue += 1.0f;
                    if (codeStreams[x].hue > 1.0f) codeStreams[x].hue -= 1.0f;
                    codeStreams[x].lastActivityTimeMs = currentTimeMs; 
#ifdef DEBUG_SERIAL
                    if (Serial) Serial.printf("Stream %d ACTIVATED: Y=%.1f, Spd=%.1f, Len=%d\n", x, codeStreams[x].currentY, codeStreams[x].speed, codeStreams[x].length);
#endif
                }
            }
        }
    }

    // 2. 渲染流
    for (int x = 0; x < NUM_STREAMS; ++x) {
        if (codeStreams[x].isActive) { // <--- RENAMED (and all subsequent uses)
            for (int l = 0; l < codeStreams[x].length; ++l) {
                int charY = floor(codeStreams[x].currentY - 1 - l);

                if (charY >= 0 && charY < MATRIX_HEIGHT) {
                    float brightnessFactor;
                    if (l == 0) { 
                        brightnessFactor = 1.0f;
                    } else { 
                        brightnessFactor = 0.8f * (1.0f - (float)l / (codeStreams[x].length > 1 ? codeStreams[x].length -1 : 1) );
                        brightnessFactor = constrain(brightnessFactor, 0.05f, 0.8f); 
                    }
                    
                    if (l > 0 && random(100) < 10) { 
                        brightnessFactor *= (random(70,101)/100.0f);
                    }

                    RgbColor charColor = HsvToRgb(codeStreams[x].hue, SATURATION, brightnessFactor);
                    
                    int logicalDrawY = MATRIX_HEIGHT - 1 - charY;
                    int ledIndex = logicalDrawY * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);

                    if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
                        strip.SetPixelColor(ledIndex, charColor); 
                    }
                }
            }
        }
    }

    strip.Show();
    delay(30); 
}