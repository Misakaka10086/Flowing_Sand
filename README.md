# Project Description: ESP32 LED Effects Controller

## 1. Project Purpose

This project is designed to control a strip of 256 NeoPixel LEDs (arranged as 64x4) to display various dynamic visual effects. It runs on an ESP32-S3 microcontroller and uses MQTT for command and control.

## 2. Hardware

*   **Microcontroller:** ESP32-S3 devkitc-1 (as specified in `platformio.ini` by `board = esp32-s3-devkitm-1`)
*   **LEDs:** 256 NeoPixel LEDs (configured as `NUM_LEDS = 64*4` in `src/main.cpp`), connected to GPIO pin 11.
*   **Accelerometer (Optional):** An ADXL345 accelerometer is included as a dependency (`adafruit/Adafruit ADXL345`), suggesting some effects might be motion-sensitive.

## 3. Software and Framework

*   **Framework:** Arduino (`framework = arduino` in `platformio.ini`)
*   **Build System:** PlatformIO

## 4. Key Libraries

*   **`makuna/NeoPixelBus`:** For controlling the NeoPixel LED strip.
*   **`marvinroger/AsyncMqttClient`:** For asynchronous MQTT communication.
*   **`bblanchon/ArduinoJson`:** For parsing and generating JSON data, likely used for MQTT message payloads.
*   **`adafruit/Adafruit ADXL345`:** For interfacing with the ADXL345 accelerometer.

## 5. Core Components

*   **`src/main.cpp`:**
    *   Initializes the ESP32, serial communication, the NeoPixel LED strip, the `EffectController`, and the `MqttController`.
    *   Contains a callback function (`onCommandReceived`) that forwards commands received via MQTT to the `EffectController`.
    *   The main `loop()` continuously calls `effectController.Update()` to refresh the current effect and `strip.Show()` to display it on the LEDs.

*   **`lib/EffectController/` (`EffectController.h`, `EffectController.cpp`):**
    *   Manages the different visual effect classes.
    *   Likely responsible for initializing, selecting, and updating the active effect.
    *   Processes commands (e.g., from MQTT) to switch effects or change effect parameters.

*   **`lib/MqttController/` (`MqttController.h`, `MqttController.cpp`):**
    *   Handles all aspects of MQTT communication (connecting to a broker, subscribing to topics, publishing messages).
    *   Receives commands from an MQTT topic and passes them to `main.cpp` (and subsequently to the `EffectController`).

*   **Individual Effect Libraries (within `lib/`):**
    *   `CodeRainEffect/`
        *   Description: Simulates the falling green code effect from 'The Matrix'.
        *   Parameters: `minSpeed`, `maxSpeed`, `minStreamLength`, `maxStreamLength`, `spawnProbability`, `minSpawnCooldownMs`, `maxSpawnCooldownMs`, `baseHue`, `hueVariation`, `saturation`, `baseBrightness`.
        *   Presets: `ClassicMatrixPreset`, `FastGlitchPreset`.
    *   `GravityBallsEffect/`
        *   Description: Simulates balls bouncing within the matrix, optionally influenced by an ADXL345 accelerometer.
        *   Parameters: `numBalls`, `gravityScale`, `dampingFactor`, `sensorDeadZone`, `restitution`, `baseBrightness`, `brightnessCyclePeriodS`, `minBrightnessScale`, `maxBrightnessScale`, `colorCyclePeriodS`, `ballColorSaturation`.
        *   Note: Uses ADXL345 accelerometer for gravity input if available.
        *   Presets: `BouncyPreset`, `PlasmaPreset`.
    *   `LavaLampEffect/`
        *   Description: Simulates the fluid dynamics of a lava lamp with merging and splitting blobs of color.
        *   Parameters: `numBlobs`, `threshold`, `baseSpeed`, `baseBrightness`, `baseColor` (string, e.g., "#FF0000"), `hueRange`.
        *   Presets: `ClassicLavaPreset`, `MercuryPreset`.
    *   `RippleEffect/`
        *   Description: Creates expanding ripples of color, like water drops.
        *   Parameters: `maxRipples`, `speed`, `acceleration`, `thickness`, `spawnIntervalS`, `maxRadius`, `randomOrigin`, `saturation`, `baseBrightness`, `sharpness`.
        *   Presets: `WaterDropPreset`, `EnergyPulsePreset`.
    *   `ScrollingTextEffect/` (includes `font8x8_basic.h`)
        *   Description: Scrolls text across the LED matrix.
        *   Parameters: `text` (String), `direction` (SCROLL_LEFT, SCROLL_RIGHT, SCROLL_UP, SCROLL_DOWN), `hue`, `saturation`, `brightness`, `scrollIntervalMs`, `charSpacing`.
        *   Presets: `DefaultPreset`, `FastBlueLeftPreset`.
    *   `ZenLightsEffect/`
        *   Description: Creates a calming effect with LEDs lighting up and fading out gently at random positions.
        *   Parameters: `maxActiveLeds`, `minDurationMs`, `maxDurationMs`, `minPeakBrightness`, `maxPeakBrightness`, `baseBrightness`, `hueMin`, `hueMax`, `saturation`, `spawnIntervalMs`.
        *   Presets: `ZenPreset`, `FireflyPreset`.
    *   Each of these libraries likely implements a specific visual pattern or animation for the LED strip, inheriting from a common base class or interface if one exists.

## 6. Communication Protocol

*   **MQTT:** The primary method for external communication and control.
    *   Commands are sent via MQTT to select different effects or modify their parameters.
    *   The MQTT topic for commands is `esp32/s3/ledcontroller/command` (defined as `MQTT_TOPIC_COMMAND` in `MqttController.h`).
    *   The ESP32 publishes its status to `esp32/s3/ledcontroller/status` (defined as `MQTT_TOPIC_STATUS` in `MqttController.h`).
    *   Messages are JSON objects.
        *   Commands can be sent to select an effect, set a preset for the current effect, or set specific parameters for the current effect.
        *   **Select an Effect:**
            *   Payload: `{"effect": "effect_name"}`
            *   Replace `"effect_name"` with one of the supported values: `"gravity_balls"`, `"zen_lights"`, `"code_rain"`, `"ripple"`, `"scrolling_text"`, `"lava_lamp"`, `"noise"`.
        *   **Set a Preset:**
            *   Payload: `{"prePara": "preset_name"}`
            *   This command applies to the currently active effect.
            *   Replace `"preset_name"` with one of the presets listed for the current effect in the 'Individual Effect Libraries' section.
        *   **Set Specific Parameters:**
            *   Payload: `{"params": {"parameter_name_1": value1, "parameter_name_2": value2, ...}}`
            *   This command applies to the currently active effect.
            *   The `params` object should contain one or more key-value pairs, where the keys are the actual parameter names for the target effect, and `value1`, `value2`, etc., are their corresponding values.
            *   Refer to the 'Individual Effect Libraries' section above for available parameters for each effect.
            *   Example of setting parameters for an arbitrary effect (replace with actual effect and parameter names/values):
                ```json
                {
                  "effect": "effect_name_here",
                  "params": {
                    "parameter_name_1": "some_string_value",
                    "parameter_name_2": 123,
                    "parameter_name_3": true
                  }
                }
                ```
            *   Note: You can also send parameters without the `"effect"` key if an effect is already active. For example, to adjust the speed of the current effect:
                ```json
                {
                  "params": {
                    "speed": 5
                  }
                }
                ```

## 7. Project Structure

*   **`platformio.ini`:** Configures the project for the PlatformIO build system (board, framework, libraries).
*   **`src/`:** Contains the main application code (`main.cpp`).
*   **`lib/`:** Contains reusable library modules, including the effect controller, MQTT controller, and individual effect implementations. Each library typically has its own `library.json` descriptor.
*   **`include/`:** May contain project-wide header files.
*   **`test/`:** Intended for unit tests (currently contains a README).
*   **`.cursor/rules/`:** Directory for AI-related context and rules. `general.mdc` likely contains general instructions for an AI.

This document aims to provide a comprehensive overview for an AI to quickly grasp the project's architecture, components, and functionality, facilitating more effective assistance with future development or troubleshooting tasks.
