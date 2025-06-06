#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <NeoPixelBus.h>
#include <math.h> // For sin(), cos(), fmod(), PI

// ADXL345 引脚定义
const int SDA_PIN = 4;
const int SCL_PIN = 5;

// WS2812 灯珠引脚定义
const int LED_PIN = 11;

// 矩阵参数
const int MATRIX_WIDTH = 8;
const int MATRIX_HEIGHT = 8;
const int NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// 小球数量
const int NUM_BALLS = 20;

// 物理参数
const float GRAVITY_SCALE = 25.0f;
const float DAMPING_FACTOR = 0.95f;
const float SENSOR_DEAD_ZONE = 0.8f;
const float BALL_RADIUS = 0.5f;
const float MIN_SEPARATION_DIST_SQ = (2 * BALL_RADIUS) * (2 * BALL_RADIUS);
const float RESTITUTION_COEFFICIENT = 0.75f; // 您代码中边界碰撞是-0.5，小球间碰撞是这个值
const float BALL_MASS = 1.0f;
const float INV_BALL_MASS = 1.0f / BALL_MASS;

// LED亮度 (0-255) - 这是灯带的整体最大亮度基准
const uint8_t BASE_BRIGHTNESS = 64;

// 小球动态亮度参数
const float MIN_BALL_BRIGHTNESS_SCALE = 0.2f;
const float MAX_BALL_BRIGHTNESS_SCALE = 1.0f;
const float BRIGHTNESS_CYCLE_PERIOD_S = 3.0f;

// 小球动态颜色参数
const float COLOR_CYCLE_PERIOD_S = 10.0f; // 一个完整的色调循环所需的时间 (秒)
const float BALL_COLOR_SATURATION = 1.0f; // 小球颜色饱和度 (0.0 to 1.0, 1.0 is full color)

// ADXL345 传感器对象
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// NeoPixelBus 对象
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);

// 小球结构体
struct Ball
{
    float x, y;
    float vx, vy;
    float brightnessFactor;
    float brightnessPhaseOffset;
    float hue;            // Current hue (0.0 to 1.0)
    float huePhaseOffset; // Hue change phase offset (0 to 2*PI)
};

Ball balls[NUM_BALLS];
unsigned long lastUpdateTime = 0;

void setup()
{
    Serial.begin(115200);
    Serial.println("ESP32 ADXL345 WS2812 Ball Sim - Color & Brightness Pulse");

    Wire.begin(SDA_PIN, SCL_PIN);

    if (!accel.begin())
    {
        Serial.println("Could not find ADXL345");
        while (1)
            ;
    }
    accel.setRange(ADXL345_RANGE_4_G);

    strip.Begin();
    strip.Show();

    // ESP32 specific: for better random numbers with random()
    // Use an unconnected analog pin, or a pin connected to a floating wire.
    // If no analog pin is easily accessible, use a combination of boot time and other factors.
    // For ESP32, `esp_random()` is generally better if you are not using Arduino's random().
    // Arduino's randomSeed(analogRead(pin)) is a common pattern.
    // Ensure the pin used for analogRead is valid for your ESP32 board (e.g., GPIOs 32-39 usually are).
    // Using a non-existent pin for analogRead might return 0 or a fixed value.
    // Let's use millis() as a fallback if analogRead isn't ideal without hardware setup.
    randomSeed(millis() + analogRead(A0)); // A0 is often GPIO36 on ESP32 DevKits

    for (int i = 0; i < NUM_BALLS; ++i)
    {
        bool positionOK;
        do
        {
            positionOK = true;
            balls[i].x = random(MATRIX_WIDTH * 100) / 100.0f;
            balls[i].y = random(MATRIX_HEIGHT * 100) / 100.0f;
            for (int j = 0; j < i; ++j)
            {
                float dx_init = balls[i].x - balls[j].x;
                float dy_init = balls[i].y - balls[j].y;
                if (dx_init * dx_init + dy_init * dy_init < MIN_SEPARATION_DIST_SQ)
                {
                    positionOK = false;
                    break;
                }
            }
        } while (!positionOK);

        balls[i].vx = 0;
        balls[i].vy = 0;

        balls[i].brightnessPhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        balls[i].brightnessFactor = MIN_BALL_BRIGHTNESS_SCALE;

        balls[i].huePhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI;
        balls[i].hue = fmod(((0.0f / 1000.0f * 2.0f * PI / COLOR_CYCLE_PERIOD_S) + balls[i].huePhaseOffset) / (2.0f * PI), 1.0f);
        if (balls[i].hue < 0)
            balls[i].hue += 1.0f;

        // Serial.printf("Ball %d init: (%.2f, %.2f), b_phase: %.2f, h_phase: %.2f\n",
        //               i, balls[i].x, balls[i].y, balls[i].brightnessPhaseOffset, balls[i].huePhaseOffset);
    }
    lastUpdateTime = millis();
}

void loop()
{
    unsigned long currentTime = millis();
    float dt = (currentTime - lastUpdateTime) / 1000.0f;
    if (dt <= 0.0001f)
        dt = 0.001f; // Avoid dt=0 or too small
    lastUpdateTime = currentTime;

    float totalTimeSeconds = currentTime / 1000.0f;

    sensors_event_t event;
    accel.getEvent(&event);
    float rawAx = event.acceleration.x;
    float rawAz = event.acceleration.z;

    float ax_eff = (abs(rawAx) < SENSOR_DEAD_ZONE) ? 0 : rawAx;
    float az_eff = (abs(rawAz) < SENSOR_DEAD_ZONE) ? 0 : rawAz;

    float forceX = -ax_eff * GRAVITY_SCALE;
    float forceY = az_eff * GRAVITY_SCALE;

    for (int i = 0; i < NUM_BALLS; ++i)
    {
        // ---- 更新亮度 ----
        float sinWaveBright = (sin((2.0f * PI / BRIGHTNESS_CYCLE_PERIOD_S * totalTimeSeconds) + balls[i].brightnessPhaseOffset) + 1.0f) / 2.0f;
        balls[i].brightnessFactor = MIN_BALL_BRIGHTNESS_SCALE + sinWaveBright * (MAX_BALL_BRIGHTNESS_SCALE - MIN_BALL_BRIGHTNESS_SCALE);

        // ---- 更新色调 (Hue) ----
        float rawHueAngle = (2.0f * PI / COLOR_CYCLE_PERIOD_S * totalTimeSeconds) + balls[i].huePhaseOffset;
        balls[i].hue = fmod(rawHueAngle / (2.0f * PI), 1.0f);
        if (balls[i].hue < 0.0f)
            balls[i].hue += 1.0f;

        // ---- 更新物理运动 ----
        balls[i].vx += forceX * dt;
        balls[i].vy += forceY * dt;
        balls[i].vx *= DAMPING_FACTOR;
        balls[i].vy *= DAMPING_FACTOR;
        balls[i].x += balls[i].vx * dt;
        balls[i].y += balls[i].vy * dt;

        // ---- 边界碰撞 ---- (Using your -0.5 factor from the provided code)
        if (balls[i].x < BALL_RADIUS)
        {
            balls[i].x = BALL_RADIUS;
            balls[i].vx *= -0.5;
        }
        else if (balls[i].x > MATRIX_WIDTH - BALL_RADIUS)
        {
            balls[i].x = MATRIX_WIDTH - BALL_RADIUS;
            balls[i].vx *= -0.5;
        }
        if (balls[i].y < BALL_RADIUS)
        {
            balls[i].y = BALL_RADIUS;
            balls[i].vy *= -0.5;
        }
        else if (balls[i].y > MATRIX_HEIGHT - BALL_RADIUS)
        {
            balls[i].y = MATRIX_HEIGHT - BALL_RADIUS;
            balls[i].vy *= -0.5;
        }
    }

    // ---- 小球间碰撞 ----
    for (int i = 0; i < NUM_BALLS; ++i)
    {
        for (int j = i + 1; j < NUM_BALLS; ++j)
        {
            float dx = balls[j].x - balls[i].x;
            float dy = balls[j].y - balls[i].y;
            float distSq = dx * dx + dy * dy;

            if (distSq < MIN_SEPARATION_DIST_SQ && distSq > 0.00001f)
            {
                float dist = sqrt(distSq);
                float nx = dx / dist;
                float ny = dy / dist;
                float rvx = balls[j].vx - balls[i].vx;
                float rvy = balls[j].vy - balls[i].vy;
                float velAlongNormal = rvx * nx + rvy * ny;

                if (velAlongNormal < 0)
                {
                    float impulseMagnitude = -(1.0f + RESTITUTION_COEFFICIENT) * velAlongNormal / (2.0f * INV_BALL_MASS);
                    balls[i].vx -= impulseMagnitude * nx * INV_BALL_MASS;
                    balls[i].vy -= impulseMagnitude * ny * INV_BALL_MASS;
                    balls[j].vx += impulseMagnitude * nx * INV_BALL_MASS;
                    balls[j].vy += impulseMagnitude * ny * INV_BALL_MASS;
                }

                float overlap = (2 * BALL_RADIUS) - dist;
                float correction_factor = 0.5f;
                balls[i].x -= nx * overlap * correction_factor;
                balls[i].y -= ny * overlap * correction_factor;
                balls[j].x += nx * overlap * correction_factor;
                balls[j].y += ny * overlap * correction_factor;
            }
        }
    }

    // ---- 渲染小球 ----
    strip.ClearTo(RgbColor(0, 0, 0));

    for (int i = 0; i < NUM_BALLS; ++i)
    {
        int pixelX = round(balls[i].x - BALL_RADIUS);
        int pixelY = round(balls[i].y - BALL_RADIUS);
        pixelX = constrain(pixelX, 0, MATRIX_WIDTH - 1);
        pixelY = constrain(pixelY, 0, MATRIX_HEIGHT - 1);
        int ledIndex = pixelY * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - pixelX);

        if (ledIndex >= 0 && ledIndex < NUM_LEDS)
        {

            HsbColor hsbColor(balls[i].hue, BALL_COLOR_SATURATION, balls[i].brightnessFactor * (BASE_BRIGHTNESS / 255.0f));
            strip.SetPixelColor(ledIndex, hsbColor);
        }
    }
    strip.Show();

    delay(5);
}