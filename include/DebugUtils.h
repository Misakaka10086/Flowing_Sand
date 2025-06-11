#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H

// Define DEBUG_FLAG here. Set to 1 to enable debug, 0 to disable.
#define DEBUG_FLAG 1

#if DEBUG_FLAG
  #include <Arduino.h> // Ensure Serial is available
  #define DEBUG_PRINTF(format, ...) Serial.printf(format, ##__VA_ARGS__)
  #define DEBUG_PRINTLN(message) Serial.println(message)
  #define DEBUG_PRINT(message) Serial.print(message)
#else
  #define DEBUG_PRINTF(format, ...)
  #define DEBUG_PRINTLN(message)
  #define DEBUG_PRINT(message)
#endif

#endif // DEBUG_UTILS_H
