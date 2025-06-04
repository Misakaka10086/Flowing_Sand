#include <NeoPixelBus.h> // 只需 NeoPixelBus 和 math
#include <math.h>

// --- 调试开关 ---
// #define DEBUG_SERIAL // 可以保留，如果想看波纹创建信息

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// NeoPixelBus 对象
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);

// --- 涟漪效果参数 ---
const int MAX_RIPPLES = 5;                 // 同时存在的最大波纹数量
const float DEFAULT_RIPPLE_SPEED = 3.0f;     // 波纹扩散速度 (单位/秒)
const float DEFAULT_RIPPLE_MAX_RADIUS = MATRIX_WIDTH * 1.2f; // 波纹最大扩散半径 (之前是1.5，1.2让它消失得稍快)
const float RIPPLE_THICKNESS = 1.8f;     // 波纹的视觉厚度 (之前是2.0)
const float RIPPLE_BRIGHTNESS_MULTIPLIER = 1.0f;   // 波纹亮度乘数 (0.0 - 1.0)
const uint8_t BASE_LED_BRIGHTNESS = 100;    // LED 的基础最大亮度

// --- 自动波纹参数 ---
const float AUTO_RIPPLE_INTERVAL_S = 2.0f; // 每隔多少秒自动产生一个波纹
unsigned long lastAutoRippleTimeMs = 0;

struct Ripple {
    bool isActive;
    float originX, originY;
    unsigned long startTimeMs;
    float speed;
    float maxRadius;
    float hue;
};

Ripple ripples[MAX_RIPPLES];
int nextRippleIndex = 0;


// HSV to RGB 转换函数
RgbColor HsvToRgb(float h, float s, float v_factor) { 
    float r_hsv, g_hsv, b_hsv;
    float actual_v = v_factor * RIPPLE_BRIGHTNESS_MULTIPLIER; 

    if (s == 0.0f) {
        r_hsv = g_hsv = b_hsv = actual_v;
    } else {
        int i = floor(h * 6.0f);
        float f = (h * 6.0f) - i;
        float p = actual_v * (1.0f - s);
        float q = actual_v * (1.0f - s * f);
        float t = actual_v * (1.0f - s * (1.0f - f));
        i = i % 6;
        switch (i) {
            case 0: r_hsv = actual_v; g_hsv = t; b_hsv = p; break;
            case 1: r_hsv = q; g_hsv = actual_v; b_hsv = p; break;
            case 2: r_hsv = p; g_hsv = actual_v; b_hsv = t; break;
            case 3: r_hsv = p; g_hsv = q; b_hsv = actual_v; break;
            case 4: r_hsv = t; g_hsv = p; b_hsv = actual_v; break;
            case 5: r_hsv = actual_v; g_hsv = p; b_hsv = q; break;
            default: r_hsv = 0; g_hsv = 0; b_hsv = 0; break;
        }
    }
    // 应用基础亮度
    uint8_t final_r = (uint8_t)(r_hsv * BASE_LED_BRIGHTNESS);
    uint8_t final_g = (uint8_t)(g_hsv * BASE_LED_BRIGHTNESS);
    uint8_t final_b = (uint8_t)(b_hsv * BASE_LED_BRIGHTNESS);

    return RgbColor(final_r, final_g, final_b);
}


void setup() {
#ifdef DEBUG_SERIAL
    Serial.begin(115200);
    while (!Serial) { delay(10); } 
    Serial.println("ESP32 Auto Ripple Effect");
    Serial.printf("LED Pin: %d, Matrix: %dx%d\n", LED_PIN, MATRIX_WIDTH, MATRIX_HEIGHT);
    Serial.printf("Auto ripple interval: %.1f s\n", AUTO_RIPPLE_INTERVAL_S);
#endif

#ifdef DEBUG_SERIAL
    Serial.println("Initializing NeoPixelBus strip...");
#endif
    strip.Begin();
    strip.Show(); // 初始化所有灯珠为灭
#ifdef DEBUG_SERIAL
    Serial.println("NeoPixelBus strip initialized.");
#endif

    for (int i = 0; i < MAX_RIPPLES; ++i) {
        ripples[i].isActive = false;
        ripples[i].speed = 0.0f; 
        ripples[i].startTimeMs = 0; 
    }
    // 使用 analogRead 在未连接的引脚上获取一些随机性，或者就用 millis()
    // randomSeed(millis() + analogRead(A0)); 
    randomSeed(millis());


#ifdef DEBUG_SERIAL
    Serial.println("Setup complete.");
#endif
    lastAutoRippleTimeMs = millis(); // 初始化上次自动波纹时间
}

// 波纹创建函数 (不再需要 "Debug" 后缀)
void createRipple(float ox, float oy, float startHue) {
    ripples[nextRippleIndex].isActive = true;
    ripples[nextRippleIndex].originX = ox;
    ripples[nextRippleIndex].originY = oy;
    ripples[nextRippleIndex].startTimeMs = millis(); 
    ripples[nextRippleIndex].speed = DEFAULT_RIPPLE_SPEED;
    ripples[nextRippleIndex].maxRadius = DEFAULT_RIPPLE_MAX_RADIUS;
    ripples[nextRippleIndex].hue = startHue;

#ifdef DEBUG_SERIAL
    if (Serial) {
        Serial.printf("Ripple %d CREATED: ox=%.1f, oy=%.1f, hue=%.2f, speed=%.2f, startTime=%lu\n",
                      nextRippleIndex, ox, oy, startHue, ripples[nextRippleIndex].speed, ripples[nextRippleIndex].startTimeMs);
    }
#endif
    nextRippleIndex = (nextRippleIndex + 1) % MAX_RIPPLES;
}

unsigned long loopCount = 0; 

void loop() {
    loopCount++;
    unsigned long currentTimeMs = millis(); 

    // 1. 自动创建波纹逻辑
    if (currentTimeMs - lastAutoRippleTimeMs >= (unsigned long)(AUTO_RIPPLE_INTERVAL_S * 1000.0f)) {
        lastAutoRippleTimeMs = currentTimeMs;
        
        float originX = MATRIX_WIDTH / 2.0f;
        float originY = MATRIX_HEIGHT / 2.0f;
        // 可以让起始点稍微随机一些，或者固定在中心
        // originX += random(-100, 101) / 100.0f; // -1 to +1 offset
        // originY += random(-100, 101) / 100.0f;
        // originX = constrain(originX, 0.5f, MATRIX_WIDTH - 0.5f);
        // originY = constrain(originY, 0.5f, MATRIX_HEIGHT - 0.5f);

        float randomHue = random(0, 1000) / 1000.0f;
        createRipple(originX, originY, randomHue);
#ifdef DEBUG_SERIAL
        if (Serial) {
            Serial.printf("[%lu] Auto-Ripple Triggered.\n", currentTimeMs);
        }
#endif
    }

    // 2. 渲染LED矩阵
    bool anyPixelSetThisFrame = false; // 用于调试 strip.Show()
    strip.ClearTo(RgbColor(0, 0, 0)); 

    for (int j_pixel = 0; j_pixel < MATRIX_HEIGHT; ++j_pixel) { 
        for (int i_pixel = 0; i_pixel < MATRIX_WIDTH; ++i_pixel) { 
            float pixelCenterX = i_pixel + 0.5f;
            float pixelCenterY = j_pixel + 0.5f;
            
            float maxIntensityForThisPixel = 0.0f;
            RgbColor colorForThisPixel(0,0,0);

            for (int r_idx = 0; r_idx < MAX_RIPPLES; ++r_idx) {
                if (ripples[r_idx].isActive) {
                    unsigned long rippleStartTime = ripples[r_idx].startTimeMs;
                    float rippleSpeed = ripples[r_idx].speed; 
                    
                    float elapsedTimeS = (float)(currentTimeMs - rippleStartTime) / 1000.0f; 
                    float currentRadius = elapsedTimeS * rippleSpeed; 

                    // 波纹消失条件
                    if (currentRadius > ripples[r_idx].maxRadius + RIPPLE_THICKNESS / 2.0f) { //  /2 so it fades out at edge
                        ripples[r_idx].isActive = false; 
#ifdef DEBUG_SERIAL
                         if(Serial && loopCount % 50 == 0) { // Less frequent deactivation log
                             Serial.printf("[%lu] R[%d] DEACTIVATED (rad=%.2f > maxRTh=%.2f)\n", 
                                           currentTimeMs, r_idx, currentRadius, ripples[r_idx].maxRadius + RIPPLE_THICKNESS / 2.0f);
                         }
#endif
                        continue; 
                    }

                    float distToOrigin = sqrt(pow(pixelCenterX - ripples[r_idx].originX, 2) + pow(pixelCenterY - ripples[r_idx].originY, 2));
                    float distToRippleEdge = abs(distToOrigin - currentRadius);

                    if (distToRippleEdge < RIPPLE_THICKNESS / 2.0f) {
                        // 平滑的亮度衰减，类似高斯波形的中心部分
                        float intensityFactor = cos((distToRippleEdge / (RIPPLE_THICKNESS / 2.0f)) * (PI / 2.0f));
                        intensityFactor = constrain(intensityFactor, 0.0f, 1.0f);


                        if (intensityFactor > maxIntensityForThisPixel) {
                            maxIntensityForThisPixel = intensityFactor;
                            colorForThisPixel = HsvToRgb(ripples[r_idx].hue, 1.0f, maxIntensityForThisPixel);
                        }
                    }
                }
            }

            if (maxIntensityForThisPixel > 0.0f) {
                // 逻辑坐标 (i_pixel, j_pixel)，(0,0) 是左下角
                // LED 索引：从右到左，从下到上
                int ledIndex = j_pixel * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - i_pixel);
                 if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
                    strip.SetPixelColor(ledIndex, colorForThisPixel);
                    anyPixelSetThisFrame = true;
                 }
            }
        }
    }

#ifdef DEBUG_SERIAL
    if (loopCount % 100 == 0 && Serial) { // Print strip.Show status less frequently
        if (anyPixelSetThisFrame) {
            Serial.printf("[%lu] strip.Show() called, pixels were set.\n", currentTimeMs);
        } else {
            Serial.printf("[%lu] strip.Show() called, NO pixels set this frame.\n", currentTimeMs);
        }
    }
#endif
    strip.Show();
    delay(20); // 控制整体帧率，约50FPS
}