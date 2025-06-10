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
    *   `GravityBallsEffect/`
    *   `LavaLampEffect/`
    *   `RippleEffect/`
    *   `ScrollingTextEffect/` (includes `font8x8_basic.h`)
    *   `ZenLightsEffect/`
    *   Each of these libraries likely implements a specific visual pattern or animation for the LED strip, inheriting from a common base class or interface if one exists.

## 6. Communication Protocol

*   **MQTT:** The primary method for external communication and control.
    *   Commands are sent via MQTT to select different effects or modify their parameters.
    *   The exact MQTT topics and message format (likely JSON) would be defined within the `MqttController` and potentially documented further or discoverable from its source code.

## 7. Project Structure

*   **`platformio.ini`:** Configures the project for the PlatformIO build system (board, framework, libraries).
*   **`src/`:** Contains the main application code (`main.cpp`).
*   **`lib/`:** Contains reusable library modules, including the effect controller, MQTT controller, and individual effect implementations. Each library typically has its own `library.json` descriptor.
*   **`include/`:** May contain project-wide header files.
*   **`test/`:** Intended for unit tests (currently contains a README).
*   **`.cursor/rules/`:** Directory for AI-related context and rules. `general.mdc` likely contains general instructions for an AI.

This document aims to provide a comprehensive overview for an AI to quickly grasp the project's architecture, components, and functionality, facilitating more effective assistance with future development or troubleshooting tasks.
