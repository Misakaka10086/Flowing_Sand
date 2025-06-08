#ifndef SCROLLING_TEXT_EFFECT_H
#define SCROLLING_TEXT_EFFECT_H

#include <NeoPixelBus.h>
#include <ArduinoJson.h> // For parsing parameters
#include <WString.h>     // For Arduino String

// Forward declaration if font8x8_basic is not included here but in .cpp
// For simplicity, we will include it in the .cpp file.

// Defines the dimensions of characters we extract from the 8x8 font
const uint8_t CHAR_DISPLAY_WIDTH = 5;
const uint8_t CHAR_DISPLAY_HEIGHT = 7;

class ScrollingTextEffect {
public:
    enum ScrollDirection {
        SCROLL_LEFT,
        SCROLL_RIGHT,
        SCROLL_UP,
        SCROLL_DOWN
    };

    struct Parameters {
        String text;
        ScrollDirection direction;
        float hue;
        float saturation;
        float brightness;
        unsigned long scrollIntervalMs; // Time between scroll steps
        uint8_t charSpacing;            // Pixels between characters
        const char* prePara;            // For preset identification
    };

    static const Parameters DefaultPreset;
    static const Parameters FastBlueLeftPreset;

    ScrollingTextEffect();
    ~ScrollingTextEffect();

    template<typename T_NeoPixelBus>
    void Begin(T_NeoPixelBus& strip) {
        _strip = &strip;
        _numLeds = strip.PixelCount();
        // Assuming 8x8 matrix
        _matrixWidth = 8;
        _matrixHeight = 8;
        
        // Apply initial parameters and calculate text dimensions
        setParameters(_params); // This will call resetScroll internally
        _lastScrollTimeMs = millis();
    }

    void Update();
    void setParameters(const Parameters& params);
    void setParameters(const char* jsonParams);
    void setPreset(const char* presetName);

private:
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod>* _strip = nullptr;
    uint16_t _numLeds = 0;
    uint8_t _matrixWidth = 0;
    uint8_t _matrixHeight = 0;

    Parameters _params;

    unsigned long _lastScrollTimeMs = 0;
    int _scrollPositionX = 0; // Current X offset of the text relative to screen left
    int _scrollPositionY = 0; // Current Y offset of the text relative to screen top
    int _actualTextPixelWidth = 0; // Total width of the rendered text string in pixels

    void resetScrollState();
    bool getCharPixel(char c, uint8_t char_x, uint8_t char_y);
    void drawPixel(int x, int y, const HsbColor& color);
};

#endif // SCROLLING_TEXT_EFFECT_H