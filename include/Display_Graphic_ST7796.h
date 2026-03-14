// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "Display_Graphic.h"
#include "PinMapping.h"
#include <Arduino.h>

#if __has_include(<Arduino_GFX_Library.h>)
#define OPENDTU_HAS_ARDUINO_GFX 1
#ifdef INLINE
#undef INLINE
#endif
#include <Arduino_GFX_Library.h>
#else
#define OPENDTU_HAS_ARDUINO_GFX 0
class Arduino_GFX;
class Arduino_HWSPI;
#endif

class DisplayGraphicST7796Class {
public:
    DisplayGraphicST7796Class();
    ~DisplayGraphicST7796Class();

    bool init(const PinMapping_t& pin);
    void setContrast(uint8_t contrast);
    void setStatus(bool turnOn);
    void setOrientation(uint8_t rotation);
    void setLocale(const String& locale);
    void setDiagramMode(DiagramMode_t mode);
    void setStartupDisplay();
    void loop(bool enablePowerSafe, bool enableScreensaver);

    bool isInitialized() const;

private:
    void drawCenteredText(const char* text, int16_t y, uint8_t textSize, uint16_t color);
    void drawTextAt(const char* text, int16_t x, int16_t y, uint8_t textSize, uint16_t color);
    void drawRightAligned(const char* text, int16_t y, uint8_t textSize, uint16_t color, int16_t rightMargin = 8);
    void drawKeyValue(const char* key, const char* value, int16_t y, uint8_t textSize, uint16_t keyColor, uint16_t valueColor, int16_t leftMargin = 8);
    void drawHLine(int16_t y, uint16_t color, int16_t margin = 12);
    void drawTextScreen(bool enableScreensaver);
    void updateLocaleStrings(const String& locale);
    void applyBacklight(bool enabled);

#if OPENDTU_HAS_ARDUINO_GFX
    Arduino_HWSPI* _bus = nullptr;
    Arduino_GFX* _display = nullptr;
#endif
    bool _initialized = false;
    bool _displayTurnedOn = true;
    uint8_t _contrast = DISPLAY_CONTRAST;
    uint8_t _rotation = DISPLAY_ROTATION;
    DiagramMode_t _diagramMode = static_cast<DiagramMode_t>(DISPLAY_DIAGRAM_MODE);
    String _display_language = DISPLAY_LOCALE;
    uint32_t _previousMillis = 0;
    uint8_t _mExtra = 0;

};
