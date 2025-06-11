#ifndef TRANSITION_UTILS_H
#define TRANSITION_UTILS_H

#include <Arduino.h> // For millis(), min(), round() if needed, though standard C++ math could be used.
#include <math.h>    // For round, fmin (though min from Arduino.h might be simpler)

// Default transition duration in milliseconds
const unsigned long DEFAULT_TRANSITION_DURATION_MS = 500;

// Linear interpolation for float
inline float lerp(float start, float end, float t) {
    // Clamp t to the range [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return start + t * (end - start);
}

// Linear interpolation for int (rounds to nearest int)
inline int lerp(int start, int end, float t) {
    // Clamp t to the range [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return static_cast<int>(round(start + t * (end - start)));
}

// It might also be useful to have a version for byte (uint8_t) if colors or similar are involved
// For now, int and float should cover most numeric parameters.
// HSB colors are often represented as floats (0-1 for H, S, B) or int (0-255 for H, S, V)
// If color structures are being interpolated, it's usually per-component.

#endif // TRANSITION_UTILS_H
