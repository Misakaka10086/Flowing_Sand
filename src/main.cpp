#include <Wire.h>
#include <Adafruit_ADXL345_U.h>
#include <Adafruit_Sensor.h>
#include <NeoPixelBus.h>
#include <math.h> // For sin() and PI
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
const int NUM_BALLS = 20; // 可以根据需要调整小球数量

// 物理参数
const float GRAVITY_SCALE = 25.0f;    // 重力影响缩放系数 (调这个值改变小球对倾斜的反应速度)
const float DAMPING_FACTOR = 0.95f;  // 阻尼系数 (0.0 - 1.0, 越接近1阻尼越小)
const float SENSOR_DEAD_ZONE = 0.8f; // 传感器死区 (m/s^2)，用于忽略微小抖动
const float BALL_RADIUS = 0.5f;      // 小球半径，用于碰撞检测 (0.5意味着小球直径为1个像素)
const float MIN_SEPARATION_DIST_SQ = (2 * BALL_RADIUS) * (2 * BALL_RADIUS); // 最小分离距离的平方
const float RESTITUTION_COEFFICIENT = 0.75f; // 恢复系数 (0.0 - 1.0), 0=完全非弹性, 1=完全弹性
const float BALL_MASS = 1.0f; // 假设所有小球质量为1，简化计算
const float INV_BALL_MASS = 1.0f / BALL_MASS; // 质量的倒数，用于冲量计算
// LED亮度 (0-255)
const uint8_t BASE_BRIGHTNESS = 64;


// 小球动态亮度参数
const float MIN_BALL_BRIGHTNESS_SCALE = 0.2f;  // 小球亮度变化的最小比例 ( نسبت به BASE_BRIGHTNESS)
const float MAX_BALL_BRIGHTNESS_SCALE = 1.0f;  // 小球亮度变化的最大比例
const float BRIGHTNESS_CYCLE_PERIOD_S = 3.0f;   // 一个完整的明暗变化周期需要的时间 (秒)
// ADXL345 传感器对象
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345); // 传感器ID随意

// NeoPixelBus 对象 (GRB颜色顺序, 800Kbps速率)
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(NUM_LEDS, LED_PIN);

struct Ball {
    float x, y;
    float vx, vy;
    // float currentBrightness; // 移除旧的，或修改为因子
    float brightnessFactor;      // 当前亮度比例 (0.0 to 1.0 based on min/max scale)
    float brightnessPhaseOffset; // 亮度变化的相位偏移 (0 to 2*PI)
};

Ball balls[NUM_BALLS];

unsigned long lastUpdateTime = 0;

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32 ADXL345 WS2812 Ball Simulation");

    // 初始化I2C
    Wire.begin(SDA_PIN, SCL_PIN);

    // 初始化ADXL345
    if (!accel.begin()) {
        Serial.println("Could not find a valid ADXL345 sensor, check wiring!");
        while (1);
    }
    accel.setRange(ADXL345_RANGE_4_G); // 设置量程, 2G, 4G, 8G, 16G. 4G通常足够

    // 初始化NeoPixelBus
    strip.Begin();
    strip.Show(); // 初始化所有灯珠为灭

    // 初始化小球位置和速度
    // 确保初始位置不重叠
    for (int i = 0; i < NUM_BALLS; ++i) {
        bool positionOK;
        do {
            positionOK = true;
            balls[i].x = random(MATRIX_WIDTH * 100) / 100.0f; // 随机初始位置
            balls[i].y = random(MATRIX_HEIGHT * 100) / 100.0f;

            // 检查与之前已放置小球的重叠
            for (int j = 0; j < i; ++j) {
                float dx = balls[i].x - balls[j].x;
                float dy = balls[i].y - balls[j].y;
                if (dx * dx + dy * dy < MIN_SEPARATION_DIST_SQ) {
                    positionOK = false;
                    break;
                }
            }
        } while (!positionOK);
        
        balls[i].vx = 0;
        balls[i].vy = 0;

        // 初始化亮度相关的参数
        balls[i].brightnessPhaseOffset = (random(0, 10000) / 10000.0f) * 2.0f * PI; // 随机相位偏移 (0 to 2*PI)
        // 初始亮度因子会在loop中第一次计算时设置
        balls[i].brightnessFactor = MIN_BALL_BRIGHTNESS_SCALE; // 或一个基于相位的初始值
        Serial.printf("Ball %d initial position: (%.2f, %.2f), phase_offset: %.2f\n", i, balls[i].x, balls[i].y, balls[i].brightnessPhaseOffset);
    }
    lastUpdateTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    float dt = (currentTime - lastUpdateTime) / 1000.0f; // 计算时间差 (秒)
    if (dt == 0) dt = 0.001f; // 防止dt为0
    lastUpdateTime = currentTime;

    float totalTimeSeconds = currentTime / 1000.0f; // 总运行时间，用于平滑亮度变化
    // 1. 读取ADXL345传感器数据
    sensors_event_t event;
    accel.getEvent(&event);

    float rawAx = event.acceleration.x;
    float rawAz = event.acceleration.z; // Z轴控制前后

    // 2. 应用传感器死区
    float ax_eff = (abs(rawAx) < SENSOR_DEAD_ZONE) ? 0 : rawAx;
    float az_eff = (abs(rawAz) < SENSOR_DEAD_ZONE) ? 0 : rawAz;

    // 3. 映射加速度到小球运动
    // X轴: 向左倾斜 (传感器X值增大) -> 小球X坐标减小
    // Z轴: 向前倾斜 (传感器Z值增大) -> 小球Y坐标增大 (假设"前"是矩阵Y轴正方向)
    float forceX = -ax_eff * GRAVITY_SCALE;
    float forceY = az_eff * GRAVITY_SCALE; // Z轴正向倾斜对应Y轴正向运动

    // 4. 更新每个小球的状态
    for (int i = 0; i < NUM_BALLS; ++i) {
        // ---- 更新亮度 ----
        // sin函数值在 -1 到 1 之间。我们把它映射到 0 到 1
        float sinWave = (sin((2.0f * PI / BRIGHTNESS_CYCLE_PERIOD_S * totalTimeSeconds) + balls[i].brightnessPhaseOffset) + 1.0f) / 2.0f; // 结果 0.0 to 1.0
        // 将 0-1 的sinWave 映射到 MIN_BALL_BRIGHTNESS_SCALE 到 MAX_BALL_BRIGHTNESS_SCALE 范围
        balls[i].brightnessFactor = MIN_BALL_BRIGHTNESS_SCALE + sinWave * (MAX_BALL_BRIGHTNESS_SCALE - MIN_BALL_BRIGHTNESS_SCALE);

        // 更新速度 (v = v0 + a*t)
        balls[i].vx += forceX * dt;
        balls[i].vy += forceY * dt;

        // 应用阻尼
        balls[i].vx *= DAMPING_FACTOR;
        balls[i].vy *= DAMPING_FACTOR;

        // 更新位置 (s = s0 + v*t)
        balls[i].x += balls[i].vx * dt;
        balls[i].y += balls[i].vy * dt;

        // 5. 处理边界碰撞
        // X轴边界
        if (balls[i].x < BALL_RADIUS) {
            balls[i].x = BALL_RADIUS;
            balls[i].vx *= -0.5; // 反弹并损失一些能量
        } else if (balls[i].x > MATRIX_WIDTH - BALL_RADIUS) {
            balls[i].x = MATRIX_WIDTH - BALL_RADIUS;
            balls[i].vx *= -0.5;
        }
        // Y轴边界
        if (balls[i].y < BALL_RADIUS) {
            balls[i].y = BALL_RADIUS;
            balls[i].vy *= -0.5;
        } else if (balls[i].y > MATRIX_HEIGHT - BALL_RADIUS) {
            balls[i].y = MATRIX_HEIGHT - BALL_RADIUS;
            balls[i].vy *= -0.5;
        }
    }

    // 6. 处理小球之间的碰撞
    for (int i = 0; i < NUM_BALLS; ++i) {
        for (int j = i + 1; j < NUM_BALLS; ++j) {
            float dx = balls[j].x - balls[i].x;
            float dy = balls[j].y - balls[i].y;
            float distSq = dx * dx + dy * dy;

            // MIN_SEPARATION_DIST_SQ is (2 * BALL_RADIUS)^2
            // distSq > 0.0001f to avoid division by zero if balls are exactly at the same spot
            if (distSq < MIN_SEPARATION_DIST_SQ && distSq > 0.00001f) {
                float dist = sqrt(distSq);

                // ---- A. Collision Response (Velocity Change) ----
                // Normal vector (points from ball i to ball j), normalized
                float nx = dx / dist;
                float ny = dy / dist;

                // Relative velocity components
                float rvx = balls[j].vx - balls[i].vx;
                float rvy = balls[j].vy - balls[i].vy;

                // Relative velocity along the normal (dot product of relative velocity and normal)
                float velAlongNormal = rvx * nx + rvy * ny;

                // Do not resolve if velocities are separating (velAlongNormal > 0)
                if (velAlongNormal < 0) {
                    // Calculate impulse magnitude
                    // J = -(1 + e) * velAlongNormal / (1/m1 + 1/m2)
                    // Since m1=m2=BALL_MASS, (1/m1 + 1/m2) = 2 * INV_BALL_MASS
                    float impulseMagnitude = -(1.0f + RESTITUTION_COEFFICIENT) * velAlongNormal / (2.0f * INV_BALL_MASS);

                    // Apply impulse to change velocities
                    // v_new = v_old + J * normal_vector / mass
                    balls[i].vx -= impulseMagnitude * nx * INV_BALL_MASS;
                    balls[i].vy -= impulseMagnitude * ny * INV_BALL_MASS;
                    balls[j].vx += impulseMagnitude * nx * INV_BALL_MASS;
                    balls[j].vy += impulseMagnitude * ny * INV_BALL_MASS;
                }

                // ---- B. Positional Correction (Separation) ----
                // This is still important to prevent sinking/sticking due to discrete time steps
                float overlap = (2 * BALL_RADIUS) - dist;
                // We want to move them apart by 'overlap' distance along the normal.
                // Each ball moves half of the overlap.
                // A slight "slop" or percentage can be used if exact correction causes jitter.
                // For simplicity, we'll correct the full overlap.
                float correction_factor = 0.5f; // Each ball takes half the correction
                
                balls[i].x -= nx * overlap * correction_factor;
                balls[i].y -= ny * overlap * correction_factor;
                balls[j].x += nx * overlap * correction_factor;
                balls[j].y += ny * overlap * correction_factor;
            }
        }
    }


    // 7. 渲染小球到LED矩阵
    strip.ClearTo(RgbColor(0, 0, 0)); // 清空画布

    for (int i = 0; i < NUM_BALLS; ++i) {
        // 将浮点位置转换为整数像素坐标 (四舍五入)
        int pixelX = round(balls[i].x - BALL_RADIUS); // 小球中心对应像素的左下角
        int pixelY = round(balls[i].y - BALL_RADIUS);

        // 确保像素坐标在矩阵范围内
        pixelX = constrain(pixelX, 0, MATRIX_WIDTH - 1);
        pixelY = constrain(pixelY, 0, MATRIX_HEIGHT - 1);

        // 根据灯珠排列计算LED索引
        // "左上角为最后一颗灯珠，右下角为第一颗灯珠"
        // "矩阵灯珠的排列顺序是从右到左，从下到上"
        // 逻辑坐标(lx, ly) 0,0是左下角.
        // (0,0) -> Y=0, X=0.  Index = 0*WIDTH + (WIDTH-1-0) = 7 (for 8x8)
        // (7,0) -> Y=0, X=7.  Index = 0*WIDTH + (WIDTH-1-7) = 0 (for 8x8, 右下角第一颗)
        // (0,7) -> Y=7, X=0.  Index = 7*WIDTH + (WIDTH-1-0) = 56+7=63 (for 8x8, 左上角最后一颗)
        int ledIndex = pixelY * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - pixelX);
        
        if (ledIndex >= 0 && ledIndex < NUM_LEDS) {
            // 根据小球的 brightnessFactor 和 BASE_BRIGHTNESS 计算实际的颜色亮度
            uint8_t actualBrightness = (uint8_t)(BASE_BRIGHTNESS * balls[i].brightnessFactor);
            
            // 创建该小球的颜色 (白色，但亮度动态)
            RgbColor individualBallColor(actualBrightness, actualBrightness, actualBrightness);
            strip.SetPixelColor(ledIndex, individualBallColor);
        }
    }
    strip.Show();

    // 打印调试信息 (可选)
    // if (millis() % 1000 < 20) { // 每秒打印一次
    //     Serial.printf("AX: %.2f, AY: %.2f, AZ: %.2f || Ball0: (%.2f, %.2f) v(%.2f, %.2f)\n",
    //                   event.acceleration.x, event.acceleration.y, event.acceleration.z,
    //                   balls[0].x, balls[0].y, balls[0].vx, balls[0].vy);
    // }

    // 控制帧率
    delay(5); // 大约 50 FPS
}