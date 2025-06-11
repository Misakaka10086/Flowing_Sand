#include "ScrollingTextEffect.h"
#include "font8x8_basic.h" // Includes the actual font data array
#include "../../../include/DebugUtils.h" // Corrected path
#include "../../../include/TransitionUtils.h" // For DEFAULT_TRANSITION_DURATION_MS
// ArduinoJson.h is included in ScrollingTextEffect.h

// Define static presets
const ScrollingTextEffect::Parameters ScrollingTextEffect::DefaultPreset = {
    .text = "Hello,world",
    .direction = SCROLL_LEFT,
    .hue = 0.33f, // Green
    .saturation = 1.0f,
    .brightness = 0.5f,
    .scrollIntervalMs = 150,
    .charSpacing = 1,
    .prePara = "Default"
};

const ScrollingTextEffect::Parameters ScrollingTextEffect::FastBlueLeftPreset = {
    .text = "ESP32 MQTT",
    .direction = SCROLL_LEFT,
    .hue = 0.66f, // Blue
    .saturation = 1.0f,
    .brightness = 0.8f,
    .scrollIntervalMs = 70,
    .charSpacing = 1,
    .prePara = "FastBlue"
};


ScrollingTextEffect::ScrollingTextEffect() {
    _activeParams = DefaultPreset;
    _targetParams = DefaultPreset;
    _oldParams = DefaultPreset;
    _effectInTransition = false;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;
    _effectTransitionStartTimeMs = 0;

    _strip = nullptr;
    // These are initialized by resetScrollState(),
    // which will be called by setParameters() during Begin() or first param change.
    _actualTextPixelWidth = 0;
    _scrollPositionX = 0;
    _scrollPositionY = 0;
    _lastScrollTimeMs = 0;
}

ScrollingTextEffect::~ScrollingTextEffect() {
    // Nothing to do for now
}

void ScrollingTextEffect::setParameters(const Parameters& params) {
    DEBUG_PRINTLN("ScrollingTextEffect::setParameters(const Parameters&) called.");

    _oldParams = _activeParams;
    _targetParams = params;

    bool needsReset = false;
    // For String, enum, and spacing parameters, direct assignment is better than trying to lerp.
    // Update _activeParams and _oldParams immediately for these non-lerped fields.
    if (_targetParams.text != _oldParams.text) { // Compare against old to see if it's a new target
        _activeParams.text = _targetParams.text; // Instant change
        _oldParams.text = _targetParams.text;    // Align old with target to prevent lerp attempt
        needsReset = true;
        DEBUG_PRINTLN("Text changed, will reset scroll.");
    }
    if (_targetParams.direction != _oldParams.direction) {
        _activeParams.direction = _targetParams.direction; // Instant change
        _oldParams.direction = _targetParams.direction;    // Align old
        needsReset = true;
        DEBUG_PRINTLN("Direction changed, will reset scroll.");
    }
    if (_targetParams.charSpacing != _oldParams.charSpacing) {
        _activeParams.charSpacing = _targetParams.charSpacing; // Instant change
        _oldParams.charSpacing = _targetParams.charSpacing;      // Align old
        needsReset = true;
        DEBUG_PRINTLN("Char spacing changed, will reset scroll.");
    }
    // Also update prePara in active/old if it changed, as it is const char*
    if (_targetParams.prePara != _oldParams.prePara) {
         _activeParams.prePara = _targetParams.prePara; // Instant change
         _oldParams.prePara = _targetParams.prePara;    // Align old
    }

    if (needsReset || _actualTextPixelWidth == 0) {
        // resetScrollState should use _activeParams which now have the new text/dir/spacing
        resetScrollState();
    }

    // Start transition for HSB and scrollIntervalMs
    _effectTransitionStartTimeMs = millis();
    _effectInTransition = true;
    _effectTransitionDurationMs = DEFAULT_TRANSITION_DURATION_MS;

    DEBUG_PRINTLN("ScrollingTextEffect transition started for HSB and scrollIntervalMs.");
}

void ScrollingTextEffect::setParameters(const char* jsonParams) {
    DEBUG_PRINTLN("ScrollingTextEffect::setParameters(json) called.");
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonParams);

    if (error) {
        DEBUG_PRINTF("ScrollingTextEffect::setParameters failed to parse JSON: %s\n", error.c_str());
        return;
    }

    Parameters newParams = _effectInTransition ? _targetParams : _activeParams;

    if (doc["text"].is<String>()) newParams.text = doc["text"].as<String>();
    // For enum, check as string then convert
    if (doc["direction"].is<String>()) {
        String dirStr = doc["direction"].as<String>();
        if (dirStr.equalsIgnoreCase("left")) newParams.direction = SCROLL_LEFT;
        else if (dirStr.equalsIgnoreCase("right")) newParams.direction = SCROLL_RIGHT;
        else if (dirStr.equalsIgnoreCase("up")) newParams.direction = SCROLL_UP;
        else if (dirStr.equalsIgnoreCase("down")) newParams.direction = SCROLL_DOWN;
    }

    if (doc["hue"].is<float>()) newParams.hue = doc["hue"].as<float>();
    if (doc["saturation"].is<float>()) newParams.saturation = doc["saturation"].as<float>();
    if (doc["brightness"].is<float>()) newParams.brightness = doc["brightness"].as<float>();
    if (doc["scrollIntervalMs"].is<unsigned long>()) newParams.scrollIntervalMs = doc["scrollIntervalMs"].as<unsigned long>();
    if (doc["charSpacing"].is<uint8_t>()) newParams.charSpacing = doc["charSpacing"].as<uint8_t>();
    
    if (doc["prePara"].is<const char*>()) {
         const char* presetStr = doc["prePara"].as<const char*>();
         if (presetStr) {
            if (strcmp(presetStr, DefaultPreset.prePara) == 0) newParams.prePara = DefaultPreset.prePara;
            else if (strcmp(presetStr, FastBlueLeftPreset.prePara) == 0) newParams.prePara = FastBlueLeftPreset.prePara;
         }
    }

    setParameters(newParams);
}

void ScrollingTextEffect::setPreset(const char* presetName) {
    DEBUG_PRINTF("ScrollingTextEffect::setPreset called with: %s\n", presetName);
    const char* currentEffectivePresetName = _effectInTransition ? _targetParams.prePara : _activeParams.prePara;
    if (currentEffectivePresetName == nullptr) currentEffectivePresetName = DefaultPreset.prePara; // Fallback

    if (strcmp(presetName, "next") == 0) {
        if (strcmp(currentEffectivePresetName, DefaultPreset.prePara) == 0) {
            setParameters(FastBlueLeftPreset);
        } else {
            setParameters(DefaultPreset);
        }
    } else if (strcmp(presetName, DefaultPreset.prePara) == 0) {
        setParameters(DefaultPreset);
    } else if (strcmp(presetName, FastBlueLeftPreset.prePara) == 0) {
        setParameters(FastBlueLeftPreset);
    } else {
        DEBUG_PRINTF("ScrollingTextEffect: Unknown preset name: %s\n", presetName);
    }
    // The setParameters call will issue its own DEBUG_PRINTLN.
}


void ScrollingTextEffect::resetScrollState() {
    if (_activeParams.text.isEmpty() || _matrixWidth == 0 || _matrixHeight == 0) {
        _actualTextPixelWidth = 0;
        _scrollPositionX = 0;
        _scrollPositionY = 0;
        return;
    }

    // Calculate total width of the text string in pixels
    _actualTextPixelWidth = 0;
    if (_activeParams.text.length() > 0) {
        _actualTextPixelWidth = _activeParams.text.length() * CHAR_DISPLAY_WIDTH;
        if (_activeParams.text.length() > 1) {
            _actualTextPixelWidth += (_activeParams.text.length() - 1) * _activeParams.charSpacing;
        }
    }
    
    // Initialize scroll position based on direction for smooth entry
    switch (_activeParams.direction) {
        case SCROLL_LEFT:
            _scrollPositionX = _matrixWidth; // Start off-screen to the right
            _scrollPositionY = 0;
            break;
        case SCROLL_RIGHT:
            _scrollPositionX = -_actualTextPixelWidth; // Start off-screen to the left
            _scrollPositionY = 0;
            break;
        case SCROLL_UP:
            _scrollPositionX = 0;
            _scrollPositionY = _matrixHeight; // Start off-screen to the bottom
            break;
        case SCROLL_DOWN:
            _scrollPositionX = 0;
            _scrollPositionY = -CHAR_DISPLAY_HEIGHT; // Start off-screen to the top
            break;
    }
    _lastScrollTimeMs = millis(); // Reset timer
}

bool ScrollingTextEffect::getCharPixel(char c, uint8_t char_x, uint8_t char_y) {
    if (char_x >= CHAR_DISPLAY_WIDTH || char_y >= CHAR_DISPLAY_HEIGHT) {
        return false; // Out of 5x7 bounds
    }

    unsigned char uc = static_cast<unsigned char>(c);
    if (uc < 32 || uc >= 128) { // Printable ASCII range U+0020 to U+007E
        uc = ' '; // Replace non-standard chars with space or '?'
    }

    // font8x8_basic has 8 rows per char, we use the first CHAR_DISPLAY_HEIGHT (7)
    // LSB is the leftmost pixel of the 8 font pixels. We use the first CHAR_DISPLAY_WIDTH (5).
    return (font8x8_basic[uc][char_y] >> char_x) & 1;
}

void ScrollingTextEffect::drawPixel(int x, int y, const HsbColor& color) {
    if (x < 0 || x >= _matrixWidth || y < 0 || y >= _matrixHeight) {
        return; // Out of matrix bounds
    }
    // Mapping: Left-top (0,0 on screen) is LED 63. Right-bottom (7,7 on screen) is LED 0.
    // Order: From right to left, then bottom to top.
    // Screen Y (0=top, 7=bottom) -> Matrix Row (7=top, 0=bottom) -> 7-y
    // Screen X (0=left, 7=right) -> Matrix Col (7=left, 0=right) -> 7-x
    int ledIndex = (_matrixHeight - 1 - y) * _matrixWidth + (_matrixWidth - 1 - x);
    _strip->SetPixelColor(ledIndex, color);
}


void ScrollingTextEffect::Update() {
    if (_effectInTransition) {
        unsigned long currentTimeMs_lerp = millis(); // Renamed to avoid conflict
        unsigned long elapsedTime = currentTimeMs_lerp - _effectTransitionStartTimeMs;
        float t = static_cast<float>(elapsedTime) / _effectTransitionDurationMs;
        t = (t < 0.0f) ? 0.0f : (t > 1.0f) ? 1.0f : t; // Clamp t

        // Interpolate HSB and scrollIntervalMs. Text, direction, charSpacing change instantly.
        _activeParams.hue = lerp(_oldParams.hue, _targetParams.hue, t);
        _activeParams.saturation = lerp(_oldParams.saturation, _targetParams.saturation, t);
        _activeParams.brightness = lerp(_oldParams.brightness, _targetParams.brightness, t);
        _activeParams.scrollIntervalMs = static_cast<unsigned long>(lerp(static_cast<int>(_oldParams.scrollIntervalMs), static_cast<int>(_targetParams.scrollIntervalMs), t));

        if (t >= 1.0f) {
            _effectInTransition = false;
            // Ensure all parts of _activeParams match _targetParams at the end
            _activeParams = _targetParams;
            DEBUG_PRINTLN("ScrollingTextEffect transition complete.");
        }
    }

    if (!_strip || _activeParams.text.isEmpty() || _actualTextPixelWidth == 0) {
        if(_strip) _strip->ClearTo(RgbColor(0));
        return;
    }

    unsigned long currentTimeMs = millis();
    if (currentTimeMs - _lastScrollTimeMs < _activeParams.scrollIntervalMs) {
        return;
    }
    _lastScrollTimeMs = currentTimeMs;

    _strip->ClearTo(RgbColor(0));
    HsbColor textColor(_activeParams.hue, _activeParams.saturation, _activeParams.brightness);

    // Update scroll position
    switch (_activeParams.direction) {
        case SCROLL_LEFT:
            _scrollPositionX--;
            if (_scrollPositionX + _actualTextPixelWidth <= 0) { // Text fully scrolled past left edge
                _scrollPositionX = _matrixWidth; // Reset to right edge
            }
            break;
        case SCROLL_RIGHT:
            _scrollPositionX++;
            if (_scrollPositionX >= _matrixWidth) { // Text start fully scrolled past right edge
                _scrollPositionX = -_actualTextPixelWidth; // Reset to left edge
            }
            break;
        case SCROLL_UP:
            _scrollPositionY--;
            if (_scrollPositionY + CHAR_DISPLAY_HEIGHT <= 0) { // Text fully scrolled past top edge
                _scrollPositionY = _matrixHeight; // Reset to bottom edge
            }
            break;
        case SCROLL_DOWN:
            _scrollPositionY++;
            if (_scrollPositionY >= _matrixHeight) { // Text fully scrolled past bottom edge
                _scrollPositionY = -CHAR_DISPLAY_HEIGHT; // Reset to top edge
            }
            break;
    }

    // Render text based on current scroll position
    int vertical_char_offset = (_matrixHeight - CHAR_DISPLAY_HEIGHT) / 2; // For H-Scroll, center char vertically
    int horizontal_text_offset = 0; // For V-Scroll, center text block horizontally
    if (_activeParams.direction == SCROLL_UP || _activeParams.direction == SCROLL_DOWN) {
        if (_actualTextPixelWidth < _matrixWidth) {
            horizontal_text_offset = (_matrixWidth - _actualTextPixelWidth) / 2;
        }
    }
    
    for (int screen_y = 0; screen_y < _matrixHeight; ++screen_y) {
        for (int screen_x = 0; screen_x < _matrixWidth; ++screen_x) {
            
            int text_canvas_x = 0; // x-coordinate on the conceptual full text string canvas
            int text_canvas_y = 0; // y-coordinate on the conceptual full text string canvas (relative to char top)

            if (_activeParams.direction == SCROLL_LEFT || _activeParams.direction == SCROLL_RIGHT) {
                text_canvas_x = screen_x - _scrollPositionX;
                text_canvas_y = screen_y - vertical_char_offset;
            } else { // SCROLL_UP || SCROLL_DOWN
                text_canvas_x = screen_x - horizontal_text_offset;
                text_canvas_y = screen_y - _scrollPositionY;
            }

            if (text_canvas_y < 0 || text_canvas_y >= CHAR_DISPLAY_HEIGHT) continue; // Not within a character's vertical span
            if (text_canvas_x < 0 || text_canvas_x >= _actualTextPixelWidth) continue; // Not within the text's horizontal span

            int char_block_width = CHAR_DISPLAY_WIDTH + _activeParams.charSpacing;
            int char_index_in_string = text_canvas_x / char_block_width;
            int x_within_char_block = text_canvas_x % char_block_width;

            if (x_within_char_block < CHAR_DISPLAY_WIDTH) { // It's a character pixel, not spacing
                if (char_index_in_string >= 0 && char_index_in_string < _activeParams.text.length()) {
                    char char_to_render = _activeParams.text.charAt(char_index_in_string);
                    if (getCharPixel(char_to_render, x_within_char_block, text_canvas_y)) {
                        drawPixel(screen_x, screen_y, textColor);
                    }
                }
            }
        }
    }
}