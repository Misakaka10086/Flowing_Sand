#include <NeoPixelBus.h>
#include <math.h> // For PI (though not strictly needed for triangle wave, good to keep if other math comes up)

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

// --- 禅意灯效参数 ---
const int MAX_ACTIVE_LEDS = 8;       
const unsigned long MIN_DURATION_MS = 10000; 
const unsigned long MAX_DURATION_MS = 20000; 
const float MIN_PEAK_BRIGHTNESS = 0.3f; 
const float MAX_PEAK_BRIGHTNESS = 0.8f; 
const unsigned long ATTEMPT_NEW_LED_INTERVAL_MS = 300; 

// 颜色参数
const float BASE_HUE_MIN = 0.29f; 
const float BASE_HUE_MAX = 0.41f; 
const float SATURATION = 0.9f;   

struct LedEffectState {
    bool isActive;
    unsigned long startTimeMs;
    unsigned long durationMs;
    float peakBrightnessFactor; 
    float hue;                  
};

LedEffectState ledStates[NUM_LEDS];
unsigned long lastAttemptTimeMs = 0;

// 自定义的 HsvToRgb 函数已被移除

void setup() {
#ifdef DEBUG_SERIAL
    Serial.begin(115200);
    while (!Serial) { delay(10); } 
    Serial.println("ESP32 Zen Lights Effect - Triangle Wave Brightness");
#endif

    strip.Begin();
    strip.Show(); 

    randomSeed(millis());

    for (int i = 0; i < NUM_LEDS; ++i) {
        ledStates[i].isActive = false;
    }
    lastAttemptTimeMs = millis();

#ifdef DEBUG_SERIAL
    Serial.println("Setup complete.");
#endif
}

int countActiveLeds() {
    int count = 0;
    for (int i = 0; i < NUM_LEDS; ++i) {
        if (ledStates[i].isActive) {
            count++;
        }
    }
    return count;
}

void tryActivateNewLed() {
    if (countActiveLeds() >= MAX_ACTIVE_LEDS) {
        return; 
    }

    int candidateLed = random(NUM_LEDS); 
    int attempts = 0;
    while (ledStates[candidateLed].isActive && attempts < NUM_LEDS * 2) { 
        candidateLed = random(NUM_LEDS); 
        attempts++;
    }

    if (!ledStates[candidateLed].isActive) { 
        ledStates[candidateLed].isActive = true; 
        ledStates[candidateLed].startTimeMs = millis(); 
        ledStates[candidateLed].durationMs = random(MIN_DURATION_MS, MAX_DURATION_MS + 1); 
        ledStates[candidateLed].peakBrightnessFactor = random(MIN_PEAK_BRIGHTNESS * 100, MAX_PEAK_BRIGHTNESS * 100 + 1) / 100.0f; 
        ledStates[candidateLed].hue = random(BASE_HUE_MIN * 1000, BASE_HUE_MAX * 1000 + 1) / 1000.0f; 

#ifdef DEBUG_SERIAL
        if (Serial) {
            Serial.printf("LED %d activated: dur=%lums, peakB=%.2f, hue=%.2f\n", 
                candidateLed, ledStates[candidateLed].durationMs,  
                ledStates[candidateLed].peakBrightnessFactor, ledStates[candidateLed].hue); 
        }
#endif
    }
}

unsigned long loopCounter = 0;

void loop() {
    unsigned long currentTimeMs = millis();
    loopCounter++;

    if (currentTimeMs - lastAttemptTimeMs >= ATTEMPT_NEW_LED_INTERVAL_MS) {
        lastAttemptTimeMs = currentTimeMs;
        tryActivateNewLed();
    }

    for (int i = 0; i < NUM_LEDS; ++i) {
        if (ledStates[i].isActive) {
            unsigned long elapsedTime = currentTimeMs - ledStates[i].startTimeMs;

            if (elapsedTime >= ledStates[i].durationMs) {
                ledStates[i].isActive = false;
                strip.SetPixelColor(i, RgbColor(0, 0, 0)); 
#ifdef DEBUG_SERIAL
                if (Serial && loopCounter % 50 == 0) Serial.printf("LED %d deactivated.\n", i);
#endif
                continue;
            }

            float progress = (float)elapsedTime / ledStates[i].durationMs; // 0.0 to 1.0
            float currentCycleBrightness;

            // ***** Triangle wave calculation *****
            if (progress < 0.5f) {
                // Rising edge (0 to 0.5 progress -> 0 to 1 brightness)
                currentCycleBrightness = progress * 2.0f;
            } else {
                // Falling edge (0.5 to 1.0 progress -> 1 to 0 brightness)
                currentCycleBrightness = (1.0f - progress) * 2.0f;
            }
            // An alternative one-liner for triangle wave (0-1-0 for progress 0-1):
            // currentCycleBrightness = 1.0f - abs(progress - 0.5f) * 2.0f; 
            // However, the if/else is perhaps more readable for this specific case.

            currentCycleBrightness *= ledStates[i].peakBrightnessFactor; 
            currentCycleBrightness = constrain(currentCycleBrightness, 0.0f, 1.0f);

            HsbColor hsbColor(ledStates[i].hue, SATURATION, currentCycleBrightness);
            strip.SetPixelColor(i, hsbColor); 

        } else {
             strip.SetPixelColor(i, RgbColor(0, 0, 0)); 
        }
    }

    strip.Show();
    delay(20); 
}