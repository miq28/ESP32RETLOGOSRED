#include <Arduino.h>
#include "config.h"
#include "can_manager.h"
#include "led_manager.h"
#include "can_driver.h"
#include "can_frame.h"
#include "SerialConsole.h"
#include "gvret_comm.h"
#include "lawicel.h"
#include "ELM327_Emulator.h"

volatile bool canPauseRX = false;

#define CAN_RING_SIZE 1024 // 512 // increase if needed (RAM cost ~ 32KB)

#define CAN_RX_STACK 4096
#define CAN_TX_STACK 6144

static volatile uint32_t ringOverflowCount = 0;

// FPS counter for debugging purposes
static volatile uint32_t frameCounter[NUM_BUSES] = {0};
static volatile uint32_t lastFPS[NUM_BUSES] = {0};

// Track TWAI RX queue high-water mark
static volatile uint32_t twaiRxQueueHigh = 0;
static volatile uint32_t twaiRxQueueFullEvents = 0;
// Track ring buffer high-water mark
static volatile uint16_t ringHighWater = 0;
// Track transport throughput
static volatile uint32_t transportFrames = 0;

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
        ringOverflowCount++;
        return;
    }

    canRing[ringHead].timestamp = micros();
    canRing[ringHead].bus = bus;
    canRing[ringHead].frame = frame;

    uint16_t used =
        (ringHead >= ringTail) ? (ringHead - ringTail)
                               : (CAN_RING_SIZE - ringTail + ringHead);

    if (used > ringHighWater)
        ringHighWater = used;

    __sync_synchronize();

    ringHead = next;
}

CANManager::CANManager()
{
}

void CANManager::setup()
{
    if (!can_init(settings.canSettings[0].nomSpeed))
    {
        DEBUG("CAN init failed\n");
        return;
    }

    busLoad[0].bitsPerQuarter = settings.canSettings[0].nomSpeed / 4;
    busLoad[0].bitsSoFar = 0;
    busLoad[0].busloadPercentage = 0;

    busLoadTimer = millis();

    // allow TWAI driver to stabilize
    delay(200);

    // Create CAN_RX task (Core 1)
    xTaskCreatePinnedToCore(
        canRxTask,
        "CAN_RX",
        CAN_RX_STACK,
        NULL,
        3,
        NULL,
        1);

    // Create CAN_TX task (Core 0)
    xTaskCreatePinnedToCore(
        transportTask,
        "CAN_TX",
        CAN_TX_STACK,
        NULL,
        2,
        NULL,
        0);
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

// void CANManager::addBits(int offset, CAN_FRAME_FD &frame)
// {
//     if (offset < 0)
//         return;
//     if (offset >= NUM_BUSES)
//         return;
//     busLoad[offset].bitsSoFar += 41 + (frame.length * 9);
//     if (frame.extended)
//         busLoad[offset].bitsSoFar += 18;
// }

void CANManager::sendFrame(CAN_FRAME &frame)
{
    can_send(frame.id, frame.extended, frame.rtr, frame.length, frame.data);
}

void CANManager::displayFrame(CAN_FRAME &frame, int whichBus)
{
    if (settings.enableLawicel && SysSettings.lawicelMode)
    {
        lawicel.sendFrameToBuffer(
            frame.id,
            frame.extended,
            frame.length,
            frame.data,
            whichBus);
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
    // CAN_FRAME_FD inFD;
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
    const int MAX_BATCH = 12;

    while (true)
    {
        int batch = 0;

        bool lawicelMode = settings.enableLawicel && SysSettings.lawicelMode;
        bool wifi = SysSettings.isWifiActive;

        while (!ringIsEmpty() && batch < MAX_BATCH)
        {
            RingItem &item = canRing[ringTail];

            if (lawicelMode)
                lawicel.sendFrameToBuffer(
                    item.frame.id,
                    item.frame.extended,
                    item.frame.length,
                    item.frame.data,
                    item.bus);
            else if (wifi)
                wifiGVRET.sendFrameToBuffer(item.frame, item.bus);
            else
                serialGVRET.sendFrameToBuffer(item.frame, item.bus);

            transportFrames++; // ← ADD HERE

            ringTail = (ringTail + 1) % CAN_RING_SIZE;
            batch++;
        }

        if (batch == 0)
            vTaskDelay(1);

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

            DEBUG("Uptime=%ud %02u:%02u:%02u Heap:%u FPS=%lu TX=%lu Ovf=%lu Used=%u Head=%u Tail=%u RingHW=%u TWAIHW=%lu Miss=%lu\n",
                  days,
                  hours,
                  minutes,
                  seconds,
                  ESP.getFreeHeap(),
                  lastFPS[0],
                  transportFrames,
                  overflows,
                  used,
                  ringHead,
                  ringTail,
                  ringHighWater,
                  twaiRxQueueHigh,
                  twaiRxQueueFullEvents);

            // reset counters once per second
            transportFrames = 0;
            twaiRxQueueHigh = 0;
            twaiRxQueueFullEvents = 0;
            ringHighWater = 0;
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
    twai_message_t msg;
    CAN_FRAME frame;
    twai_status_info_t st;

    twai_get_status_info(&st);
    DEBUG("TWAI state=%d\n", st.state);

    while (true)
    {
        if (canPauseRX)
        {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        if (twai_receive(&msg, pdMS_TO_TICKS(1)) == ESP_OK)
        {
            do
            {
                ledNotifyCanActivity();

                frame.id = msg.identifier;
                frame.extended = msg.extd;
                frame.rtr = msg.rtr;
                frame.length = msg.data_length_code;

                for (int i = 0; i < frame.length; i++)
                    frame.data[i] = msg.data[i];

                canManager.addBits(0, frame);
                pushFrame(frame, 0);

                frameCounter[0]++;

                // ---- TWAI queue diagnostics ----
                if (twai_get_status_info(&st) == ESP_OK)
                {
                    if (st.msgs_to_rx > twaiRxQueueHigh)
                        twaiRxQueueHigh = st.msgs_to_rx;

                    if (st.rx_missed_count > 0)
                        twaiRxQueueFullEvents += st.rx_missed_count;
                }

            } while (twai_receive(&msg, 0) == ESP_OK);
        }
    }
}