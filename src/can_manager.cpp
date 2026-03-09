#include <Arduino.h>
#include "config.h"
#include "can_manager.h"
#include "led_manager.h"
#include "esp32_can.h"
#include "SerialConsole.h"
#include "gvret_comm.h"
#include "lawicel.h"
#include "ELM327_Emulator.h"

volatile bool canPauseRX = false;

#define CAN_RING_SIZE 1024 // 512 // increase if needed (RAM cost ~ 32KB)

static volatile uint32_t ringOverflowCount = 0;

// FPS counter for debugging purposes
static volatile uint32_t frameCounter[NUM_BUSES] = {0};
static volatile uint32_t lastFPS[NUM_BUSES] = {0};

struct RingItem
{
    uint32_t timestamp;
    uint8_t bus;
    CAN_FRAME frame;
};

static RingItem canRing[CAN_RING_SIZE];
static volatile uint16_t ringHead = 0;
static volatile uint16_t ringTail = 0;

static inline bool ringIsFull()
{
    return ((ringHead + 1) % CAN_RING_SIZE) == ringTail;
}

static inline bool ringIsEmpty()
{
    return ringHead == ringTail;
}

static inline void pushFrame(const CAN_FRAME &frame, uint8_t bus)
{
    uint16_t next = (ringHead + 1) % CAN_RING_SIZE;

    if (next == ringTail)
    {
        ringOverflowCount = ringOverflowCount + 1; // count overflow
        return;                                    // drop newest
    }

    canRing[ringHead].timestamp = micros();
    canRing[ringHead].bus = bus;
    canRing[ringHead].frame = frame;

    ringHead = next;
}

CANManager::CANManager()
{
}

void CANManager::setup()
{
    for (int i = 0; i < SysSettings.numBuses; i++)
    {
        if (settings.canSettings[i].enabled)
        {
            canBuses[i]->enable();
            if ((settings.canSettings[i].fdMode == 0) || !canBuses[i]->supportsFDMode())
            {
                canBuses[i]->begin(settings.canSettings[i].nomSpeed, 255);
                DEBUG("Enabled CAN%u with speed %u\n", i, settings.canSettings[i].nomSpeed);
                if ((i == 0) && (settings.systemType == 2))
                {
                    digitalWrite(SW_EN, HIGH); // MUST be HIGH to use CAN0 channel
                    DEBUG("Enabling SWCAN Mode\n");
                }
                if ((i == 1) && (settings.systemType == 2))
                {
                    digitalWrite(SW_EN, LOW); // MUST be LOW to use CAN1 channel
                    DEBUG("Enabling CAN1 will force CAN0 off.\n");
                }
            }
            else
            {
                canBuses[i]->beginFD(settings.canSettings[i].nomSpeed, settings.canSettings[i].fdSpeed);
                DEBUG("Enabled CAN%u In FD Mode With Nominal Speed %u and Data Speed %u",
                      i, settings.canSettings[i].nomSpeed, settings.canSettings[i].fdSpeed);
            }

            if (settings.canSettings[i].listenOnly)
            {
                canBuses[i]->setListenOnlyMode(true);
            }
            else
            {
                canBuses[i]->setListenOnlyMode(false);
            }
            canBuses[i]->watchFor();
        }
        else
        {
            canBuses[i]->disable();
        }
    }

    for (int j = 0; j < NUM_BUSES; j++)
    {
        busLoad[j].bitsPerQuarter = settings.canSettings[j].nomSpeed / 4;
        busLoad[j].bitsSoFar = 0;
        busLoad[j].busloadPercentage = 0;
        if (busLoad[j].bitsPerQuarter == 0)
            busLoad[j].bitsPerQuarter = 125000;
    }

    busLoadTimer = millis();
}

void CANManager::addBits(int offset, CAN_FRAME &frame)
{
    if (offset < 0)
        return;
    if (offset >= NUM_BUSES)
        return;
    busLoad[offset].bitsSoFar += 41 + (frame.length * 9);
    if (frame.extended)
        busLoad[offset].bitsSoFar += 18;
}

void CANManager::addBits(int offset, CAN_FRAME_FD &frame)
{
    if (offset < 0)
        return;
    if (offset >= NUM_BUSES)
        return;
    busLoad[offset].bitsSoFar += 41 + (frame.length * 9);
    if (frame.extended)
        busLoad[offset].bitsSoFar += 18;
}

void CANManager::sendFrame(CAN_COMMON *bus, CAN_FRAME &frame)
{
    int whichBus = 0;
    for (int i = 0; i < NUM_BUSES; i++)
        if (canBuses[i] == bus)
            whichBus = i;
    bus->sendFrame(frame);
    addBits(whichBus, frame);
}

void CANManager::sendFrame(CAN_COMMON *bus, CAN_FRAME_FD &frame)
{
    int whichBus = 0;
    for (int i = 0; i < NUM_BUSES; i++)
        if (canBuses[i] == bus)
            whichBus = i;
    bus->sendFrameFD(frame);
    addBits(whichBus, frame);
}

void CANManager::displayFrame(CAN_FRAME &frame, int whichBus)
{
    if (settings.enableLawicel && SysSettings.lawicelMode)
    {
        lawicel.sendFrameToBuffer(frame, whichBus);
    }
    else
    {
        if (SysSettings.isWifiActive)
            wifiGVRET.sendFrameToBuffer(frame, whichBus);
        else
            serialGVRET.sendFrameToBuffer(frame, whichBus);
    }
}

void CANManager::displayFrame(CAN_FRAME_FD &frame, int whichBus)
{
    if (settings.enableLawicel && SysSettings.lawicelMode)
    {
        // lawicel.sendFrameToBuffer(frame, whichBus);
    }
    else
    {
        if (SysSettings.isWifiActive)
            wifiGVRET.sendFrameToBuffer(frame, whichBus);
        else
            serialGVRET.sendFrameToBuffer(frame, whichBus);
    }
}

void CANManager::loop()
{
    CAN_FRAME incoming;
    CAN_FRAME_FD inFD;
    // size_t wifiLength = wifiGVRET.numAvailableBytes();
    // size_t serialLength = serialGVRET.numAvailableBytes();
    // size_t maxLength = (wifiLength > serialLength) ? wifiLength : serialLength;

    if (millis() > (busLoadTimer + 250))
    {
        busLoadTimer = millis();
        busLoad[0].busloadPercentage = ((busLoad[0].busloadPercentage * 3) + (((busLoad[0].bitsSoFar * 1000) / busLoad[0].bitsPerQuarter) / 10)) / 4;
        // Force busload percentage to be at least 1% if any traffic exists at all. This forces the LED to light up for any traffic.
        if (busLoad[0].busloadPercentage == 0 && busLoad[0].bitsSoFar > 0)
            busLoad[0].busloadPercentage = 1;
        busLoad[0].bitsPerQuarter = settings.canSettings[0].nomSpeed / 4;
        busLoad[0].bitsSoFar = 0;
        if (busLoad[0].busloadPercentage > busLoad[1].busloadPercentage)
        {
            // updateBusloadLED(busLoad[0].busloadPercentage);
        }
        else
        {
            // updateBusloadLED(busLoad[1].busloadPercentage);
        }
    }
}

void transportTask(void *arg)
{
    while (true)
    {
        if (!ringIsEmpty())
        {
            RingItem &item = canRing[ringTail];

            if (settings.enableLawicel && SysSettings.lawicelMode)
            {
                lawicel.sendFrameToBuffer(item.frame, item.bus);
            }
            else
            {
                if (SysSettings.isWifiActive)
                    wifiGVRET.sendFrameToBuffer(item.frame, item.bus);
                else
                    serialGVRET.sendFrameToBuffer(item.frame, item.bus);
            }

            ringTail = (ringTail + 1) % CAN_RING_SIZE;
        }
        else
        {
            vTaskDelay(1);
        }

        // print ringOverflowCount every 1 seconds for debugging purposes
        // ---- STATS BLOCK MUST BE INSIDE LOOP ----
        static uint32_t lastPrint = 0;

        if (millis() - lastPrint > 1000)
        {
            lastPrint = millis();

            uint32_t overflows = ringOverflowCount;
            ringOverflowCount = 0;

            // print stats: overflows, used, head, tail, free heap
            uint16_t used =
                (ringHead >= ringTail) ? (ringHead - ringTail) : (CAN_RING_SIZE - ringTail + ringHead);

            // print FPS counter
            for (int b = 0; b < SysSettings.numBuses; b++)
            {
                lastFPS[b] = frameCounter[b];
                frameCounter[b] = 0;
            }

            // print uptime in days, hours, minutes, seconds
            uint64_t uptimeUs = esp_timer_get_time();
            uint64_t uptimeSec = uptimeUs / 1000000ULL;

            uint32_t days = uptimeSec / 86400;
            uint32_t hours = (uptimeSec % 86400) / 3600;
            uint32_t minutes = (uptimeSec % 3600) / 60;
            uint32_t seconds = uptimeSec % 60;

            DEBUG("Uptime=%ud %02u:%02u:%02u Heap:%u FPS0=%lu Overflow=%lu Used=%u Head=%u Tail=%u\n",
                  days,
                  hours,
                  minutes,
                  seconds,
                  ESP.getFreeHeap(),
                  lastFPS[0],
                  overflows,
                  used,
                  ringHead,
                  ringTail);
        }

        //--- RGB LED BUSLOAD INDICATOR (for debugging purposes) ---
        static uint32_t lastLedUpdate = 0;

        if (millis() - lastLedUpdate > 30)
        {
            lastLedUpdate = millis();
            ledTaskUpdate();
        }
    }
}

void canRxTask(void *arg)
{
    CAN_FRAME incoming;

    while (true)
    {
        if (canPauseRX)
        {
            vTaskDelay(5);
            continue;
        }

        for (int i = 0; i < NUM_BUSES; i++)
        {
            if (i >= SysSettings.numBuses)
                continue;

            CAN_COMMON *bus = canBuses[i];

            if (!bus)
                continue;
            if (!settings.canSettings[i].enabled)
                continue;

            if (bus->available() > 0)
            {
                ledNotifyCanActivity();

                if (bus->read(incoming))
                {
                    canManager.addBits(i, incoming);
                    pushFrame(incoming, i);
                    frameCounter[i] = frameCounter[i] + 1;
                }
            }
        }

        vTaskDelay(1); // critical: do NOT use taskYIELD here
    }
}