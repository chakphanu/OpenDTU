// SPDX-License-Identifier: GPL-2.0-or-later
#include "Display_Graphic_ST7796.h"
#include "Datastore.h"
#include "I18n.h"
#include "NetworkSettings.h"
#include <Hoymiles.h>
#include <time.h>

#ifndef DISPLAY_MOSI
#define DISPLAY_MOSI GPIO_NUM_NC
#endif

#ifndef DISPLAY_MISO
#define DISPLAY_MISO GPIO_NUM_NC
#endif

#ifndef DISPLAY_BL
#define DISPLAY_BL GPIO_NUM_NC
#endif

#ifndef DISPLAY_WIDTH
#define DISPLAY_WIDTH 222
#endif

#ifndef DISPLAY_HEIGHT
#define DISPLAY_HEIGHT 480
#endif

#ifndef DISPLAY_OFFSET_X1
#define DISPLAY_OFFSET_X1 49
#endif

#ifndef DISPLAY_OFFSET_Y1
#define DISPLAY_OFFSET_Y1 0
#endif

#ifndef DISPLAY_OFFSET_X2
#define DISPLAY_OFFSET_X2 49
#endif

#ifndef DISPLAY_OFFSET_Y2
#define DISPLAY_OFFSET_Y2 0
#endif

#if OPENDTU_HAS_ARDUINO_GFX
#include <SPI.h>
#endif

namespace {
constexpr uint32_t PowerSafeIntervalMs = 2UL * 60000UL;
constexpr uint8_t BacklightChannel = 1;
constexpr uint16_t BacklightFrequencyHz = 2000;
constexpr uint8_t BacklightResolutionBits = 8;

#if OPENDTU_HAS_ARDUINO_GFX
constexpr uint16_t ColorBackground = BLACK;
constexpr uint16_t ColorPrimary = WHITE;
constexpr uint16_t ColorAccent = CYAN;
constexpr uint16_t ColorMuted = LIGHTGREY;
constexpr uint16_t ColorWarning = ORANGE;
#else
constexpr uint16_t ColorBackground = 0;
constexpr uint16_t ColorPrimary = 0;
constexpr uint16_t ColorAccent = 0;
constexpr uint16_t ColorMuted = 0;
constexpr uint16_t ColorWarning = 0;
#endif

// Respect order in translation lists.
constexpr uint8_t I18N_LOCALE_EN = 0;
constexpr uint8_t I18N_LOCALE_DE = 1;
constexpr uint8_t I18N_LOCALE_FR = 2;

static const char* const i18n_offline[] = { "Offline", "Offline", "Offline" };
static const char* const i18n_current_power_w[] = { "%.0f W", "%.0f W", "%.0f W" };
static const char* const i18n_current_power_kw[] = { "%.1f kW", "%.1f kW", "%.1f kW" };
static const char* const i18n_yield_today_wh[] = { "today: %4.0f Wh", "Heute: %4.0f Wh", "auj.: %4.0f Wh" };
static const char* const i18n_yield_today_kwh[] = { "today: %.1f kWh", "Heute: %.1f kWh", "auj.: %.1f kWh" };
static const char* const i18n_yield_total_kwh[] = { "total: %.1f kWh", "Ges.: %.1f kWh", "total: %.1f kWh" };
static const char* const i18n_yield_total_mwh[] = { "total: %.0f kWh", "Ges.: %.0f kWh", "total: %.0f kWh" };
static const char* const i18n_date_format[] = { "%m/%d/%Y %H:%M", "%d.%m.%Y %H:%M", "%d/%m/%Y %H:%M" };
} // namespace

DisplayGraphicST7796Class::DisplayGraphicST7796Class() = default;

DisplayGraphicST7796Class::~DisplayGraphicST7796Class()
{
#if OPENDTU_HAS_ARDUINO_GFX
    delete _display;
    delete _bus;
#endif
}

bool DisplayGraphicST7796Class::init(const PinMapping_t& pin)
{
#if !OPENDTU_HAS_ARDUINO_GFX
    (void)pin;
    return false;
#else
    if (pin.display_data == GPIO_NUM_NC || pin.display_cs == GPIO_NUM_NC || pin.display_clk == GPIO_NUM_NC
        || DISPLAY_MOSI == GPIO_NUM_NC) {
        return false;
    }

    SPI.begin(static_cast<int8_t>(pin.display_clk), static_cast<int8_t>(DISPLAY_MISO), static_cast<int8_t>(DISPLAY_MOSI),
        static_cast<int8_t>(pin.display_cs));

    _bus = new Arduino_HWSPI(
        static_cast<int8_t>(pin.display_data),
        static_cast<int8_t>(pin.display_cs),
        static_cast<int8_t>(pin.display_clk),
        static_cast<int8_t>(DISPLAY_MOSI),
        static_cast<int8_t>(DISPLAY_MISO));

    _display = new Arduino_ST7796(
        _bus,
        pin.display_reset == GPIO_NUM_NC ? -1 : static_cast<int8_t>(pin.display_reset),
        _rotation,
        true,
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        DISPLAY_OFFSET_X1,
        DISPLAY_OFFSET_Y1,
        DISPLAY_OFFSET_X2,
        DISPLAY_OFFSET_Y2);

    if (DISPLAY_BL != GPIO_NUM_NC) {
        ledcAttachPin(DISPLAY_BL, BacklightChannel);
        ledcSetup(BacklightChannel, BacklightFrequencyHz, BacklightResolutionBits);
    }

    _initialized = _display->begin();
    if (!_initialized) {
        return false;
    }

    updateLocaleStrings(_display_language);
    setOrientation(_rotation);
    setContrast(_contrast);
    setStatus(true);
    _previousMillis = millis();
    setStartupDisplay();
    return true;
#endif
}

void DisplayGraphicST7796Class::setContrast(uint8_t contrast)
{
    _contrast = contrast;
    applyBacklight(_displayTurnedOn);
}

void DisplayGraphicST7796Class::setStatus(bool turnOn)
{
    _displayTurnedOn = turnOn;
    applyBacklight(turnOn);
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) {
        return;
    }
    if (turnOn) {
        _display->displayOn();
    } else {
        _display->displayOff();
    }
#endif
}

void DisplayGraphicST7796Class::setOrientation(uint8_t rotation)
{
    _rotation = rotation & 0x03;
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display != nullptr) {
        // The T-Connect-Pro ST7796 panel is mounted 90 degrees offset
        // compared to OpenDTU's existing display rotation convention.
        _display->setRotation((_rotation + 1) & 0x03);
    }
#endif
}

void DisplayGraphicST7796Class::setLocale(const String& locale)
{
    _display_language = locale;
    updateLocaleStrings(locale);
}

void DisplayGraphicST7796Class::setDiagramMode(DiagramMode_t mode)
{
    if (mode < DiagramMode_t::DisplayMode_Max) {
        _diagramMode = mode;
    }
}

void DisplayGraphicST7796Class::setStartupDisplay()
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) {
        return;
    }

    _display->fillScreen(ColorBackground);
    drawCenteredText("OpenDTU", 70, 4, ColorAccent);
    drawCenteredText("T-Connect-Pro", 110, 2, ColorPrimary);
    drawCenteredText("ST7796 display active", 140, 1, ColorMuted);
#endif
}

void DisplayGraphicST7796Class::loop(bool enablePowerSafe, bool enableScreensaver)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) {
        return;
    }

    bool displayPowerSave = false;
    if (!Datastore.getIsAtLeastOneReachable()) {
        if (millis() - _previousMillis >= PowerSafeIntervalMs) {
            displayPowerSave = enablePowerSafe;
        }
    } else {
        _previousMillis = millis();
    }

    if (!_displayTurnedOn) {
        displayPowerSave = true;
    }

    if (displayPowerSave) {
        applyBacklight(false);
        _display->displayOff();
        return;
    }

    _display->displayOn();
    applyBacklight(true);
    drawTextScreen(enableScreensaver);
    _mExtra++;
#else
    (void)enablePowerSafe;
    (void)enableScreensaver;
#endif
}

bool DisplayGraphicST7796Class::isInitialized() const
{
    return _initialized;
}

void DisplayGraphicST7796Class::drawCenteredText(const char* text, int16_t y, uint8_t textSize, uint16_t color)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) {
        return;
    }
    _display->setTextSize(textSize);
    _display->setTextColor(color, ColorBackground);
    int16_t x1, y1;
    uint16_t tw, th;
    _display->getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
    int16_t x = (_display->width() - static_cast<int16_t>(tw)) / 2;
    if (x < 0) x = 0;
    _display->setCursor(x, y);
    _display->print(text);
#else
    (void)text; (void)y; (void)textSize; (void)color;
#endif
}

void DisplayGraphicST7796Class::drawTextAt(const char* text, int16_t x, int16_t y, uint8_t textSize, uint16_t color)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) return;
    _display->setTextSize(textSize);
    _display->setTextColor(color, ColorBackground);
    _display->setCursor(x, y);
    _display->print(text);
#else
    (void)text; (void)x; (void)y; (void)textSize; (void)color;
#endif
}

void DisplayGraphicST7796Class::drawRightAligned(const char* text, int16_t y, uint8_t textSize, uint16_t color, int16_t rightMargin)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) return;
    _display->setTextSize(textSize);
    _display->setTextColor(color, ColorBackground);
    int16_t x1, y1;
    uint16_t tw, th;
    _display->getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
    int16_t x = _display->width() - static_cast<int16_t>(tw) - rightMargin;
    if (x < 0) x = 0;
    _display->setCursor(x, y);
    _display->print(text);
#else
    (void)text; (void)y; (void)textSize; (void)color; (void)rightMargin;
#endif
}

void DisplayGraphicST7796Class::drawKeyValue(const char* key, const char* value, int16_t y,
    uint8_t textSize, uint16_t keyColor, uint16_t valueColor, int16_t leftMargin)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) return;
    drawTextAt(key, leftMargin, y, textSize, keyColor);
    drawRightAligned(value, y, textSize, valueColor);
#else
    (void)key; (void)value; (void)y; (void)textSize;
    (void)keyColor; (void)valueColor; (void)leftMargin;
#endif
}

void DisplayGraphicST7796Class::drawHLine(int16_t y, uint16_t color, int16_t margin)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (_display == nullptr) return;
    _display->drawFastHLine(margin, y, _display->width() - margin * 2, color);
#else
    (void)y; (void)color; (void)margin;
#endif
}

void DisplayGraphicST7796Class::drawTextScreen(bool enableScreensaver)
{
#if OPENDTU_HAS_ARDUINO_GFX
    char val[48];
    constexpr int16_t M = 10; // margin
    const int16_t W = _display->width();
    const int16_t H = _display->height();
    const int16_t MID = W / 2;

    _display->fillScreen(ColorBackground);

    // ============================================================
    // LEFT PANEL (0..MID): Power + Yield
    // ============================================================

    // Power (large)
    if (Datastore.getIsAtLeastOneReachable()) {
        const float watts = Datastore.getTotalAcPowerEnabled();
        if (watts > 999.0f) {
            snprintf(val, sizeof(val), "%.1f", watts / 1000.0f);
        } else {
            snprintf(val, sizeof(val), "%.0f", watts);
        }
        drawTextAt(val, M, 10, 5, ColorAccent);
        // Unit label
        drawTextAt(watts > 999.0f ? "kW" : "W", M, 55, 2, ColorMuted);
    } else {
        drawTextAt("Offline", M, 20, 3, ColorWarning);
    }

    // Divider under power
    _display->drawFastHLine(M, 80, MID - M * 2, DARKGREY);

    // Today yield
    {
        const float wToday = Datastore.getTotalAcYieldDayEnabled();
        drawTextAt("Today", M, 90, 1, ColorMuted);
        if (wToday >= 10000.0f) {
            snprintf(val, sizeof(val), "%.1f kWh", wToday / 1000.0f);
        } else {
            snprintf(val, sizeof(val), "%.0f Wh", wToday);
        }
        drawTextAt(val, M, 106, 2, ColorPrimary);
    }

    // Total yield
    {
        const float wTotal = Datastore.getTotalAcYieldTotalEnabled();
        drawTextAt("Total", M, 136, 1, ColorMuted);
        if (wTotal >= 1000.0f) {
            snprintf(val, sizeof(val), "%.0f kWh", wTotal);
        } else {
            snprintf(val, sizeof(val), "%.1f kWh", wTotal);
        }
        drawTextAt(val, M, 152, 2, ColorPrimary);
    }

    // ============================================================
    // Vertical divider
    // ============================================================
    _display->drawFastVLine(MID, M, H - M * 2, DARKGREY);

    // ============================================================
    // RIGHT PANEL (MID..W): Status + Radio
    // ============================================================
    const int16_t R = MID + M; // right panel left edge

    // Inverter count
    {
        const size_t total = Hoymiles.getNumInverters();
        uint8_t online = 0;
        for (size_t i = 0; i < total; i++) {
            auto inv = Hoymiles.getInverterByPos(i);
            if (inv && inv->isReachable()) {
                online++;
            }
        }
        const bool allOk = online == total && total > 0;
        drawTextAt("Inverters", R, 10, 1, ColorMuted);
        snprintf(val, sizeof(val), "%u / %u", online, static_cast<unsigned>(total));
        drawTextAt(val, R, 24, 3, allOk ? ColorAccent : ColorWarning);
    }

    // Divider
    _display->drawFastHLine(R, 55, W - R - M, DARKGREY);

    // Radio success rate
    if (Hoymiles.getNumInverters() > 0) {
        auto inv = Hoymiles.getInverterByPos(0);
        if (inv) {
            const uint32_t ok = inv->RadioStats.RxSuccess;
            const uint32_t fail = inv->RadioStats.RxFailNoAnswer
                + inv->RadioStats.RxFailPartialAnswer
                + inv->RadioStats.RxFailCorruptData;
            const uint32_t attempts = ok + fail;
            const float rate = attempts > 0 ? 100.0f * ok / attempts : 0.0f;
            const bool good = rate >= 90.0f;

            drawTextAt("Radio", R, 62, 1, ColorMuted);
            snprintf(val, sizeof(val), "%.0f%%", rate);
            drawTextAt(val, R, 76, 3, good ? ColorAccent : ColorWarning);

            snprintf(val, sizeof(val), "%d dBm", static_cast<int>(inv->getLastRssi()));
            drawTextAt(val, R + 90, 76, 2, ColorMuted);

            snprintf(val, sizeof(val), "%lu / %lu",
                static_cast<unsigned long>(ok), static_cast<unsigned long>(attempts));
            drawTextAt(val, R, 106, 1, ColorMuted);

            // D6 + retransmit
            _display->drawFastHLine(R, 120, W - R - M, DARKGREY);

            snprintf(val, sizeof(val), "D6  tx:%lu  rx:%lu",
                static_cast<unsigned long>(inv->RadioStats.TxD6Sent),
                static_cast<unsigned long>(inv->RadioStats.RxD6Received));
            drawTextAt(val, R, 128, 1, ColorMuted);

            snprintf(val, sizeof(val), "Retry: %lu",
                static_cast<unsigned long>(inv->RadioStats.TxReRequestFragment));
            drawTextAt(val, R, 140, 1, ColorMuted);

            // Hop pattern tracking
            {
                const char* patNames[] = { "P1", "P2", "P3" };
                const char* cur = (inv->RadioStats.LastHopPattern >= 0 && inv->RadioStats.LastHopPattern < 3)
                    ? patNames[inv->RadioStats.LastHopPattern] : "---";
                const char* pred = (inv->RadioStats.PredictedHopPattern >= 0 && inv->RadioStats.PredictedHopPattern < 3)
                    ? patNames[inv->RadioStats.PredictedHopPattern] : "---";
                snprintf(val, sizeof(val), "Hop %s>%s  %u/%u rx",
                    cur, pred,
                    inv->RadioStats.LastBurstRxCount,
                    inv->RadioStats.LastFragCount);
                drawTextAt(val, R, 152, 1, ColorMuted);
            }
        }
    }

    // ============================================================
    // BOTTOM BAR: IP + DateTime (full width)
    // ============================================================
    _display->drawFastHLine(M, H - 22, W - M * 2, DARKGREY);

    if (NetworkSettings.localIP()) {
        drawTextAt(NetworkSettings.localIP().toString().c_str(), M, H - 16, 2, ColorMuted);
    }
    {
        time_t now = time(nullptr);
        strftime(val, sizeof(val), "%Y-%m-%d %H:%M", localtime(&now));
        drawRightAligned(val, H - 16, 2, ColorMuted, M);
    }
#else
    (void)enableScreensaver;
#endif
}

void DisplayGraphicST7796Class::updateLocaleStrings(const String& locale)
{
    (void)locale;
}

void DisplayGraphicST7796Class::applyBacklight(bool enabled)
{
#if OPENDTU_HAS_ARDUINO_GFX
    if (DISPLAY_BL == GPIO_NUM_NC) {
        return;
    }

    const uint8_t duty = enabled ? static_cast<uint8_t>((static_cast<uint16_t>(_contrast) * 255U) / 100U) : 0U;
    ledcWrite(BacklightChannel, duty);
#else
    (void)enabled;
#endif
}
