#include "led_manager.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define RGB_BUILTIN 4
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define RGB_BUILTIN 48
#endif

static volatile bool wifiConnected = false;
static volatile bool canError = false;
static volatile uint32_t lastCanActivity = 0;

static uint8_t ledBrightness = 180;

#define CAN_ACTIVITY_TIMEOUT 120   // ms
#define CAN_PULSE_PERIOD     200   // ms (pulse effect)

static uint8_t scale(uint8_t v)
{
    return (uint16_t)v * ledBrightness / 255;
}

static void setColor(uint8_t r, uint8_t g, uint8_t b)
{
    rgbLedWrite(
        RGB_BUILTIN,
        scale(r),
        scale(g),
        scale(b)
    );
}

static void updateLED()
{
    // Highest priority: CAN error
    if (canError)
    {
        setColor(255, 0, 0);   // Red
        return;
    }

    bool canActive = (millis() - lastCanActivity) < CAN_ACTIVITY_TIMEOUT;

    if (canActive)
    {
        // Yellow pulse effect
        uint32_t phase = millis() % CAN_PULSE_PERIOD;
        uint8_t pulse = (phase < (CAN_PULSE_PERIOD / 2)) ? 255 : 120;
        setColor(pulse, pulse, 0);
        return;
    }

    if (wifiConnected)
    {
        setColor(0, 0, 255);   // Blue steady
    }
    else
    {
        setColor(0, 0, 0);     // Off
    }
}

void ledInit(uint8_t brightness)
{
    ledBrightness = brightness;
    pinMode(RGB_BUILTIN, OUTPUT);
    setColor(0, 0, 0);
}

void ledSetWifiConnected(bool connected)
{
    wifiConnected = connected;
}

void ledNotifyCanActivity()
{
    lastCanActivity = millis();
}

void ledSetCanError(bool errorActive)
{
    canError = errorActive;
}

void ledTaskUpdate()
{
    updateLED();
}