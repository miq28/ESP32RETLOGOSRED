#pragma once
#include "config.h"

typedef struct
{
    uint32_t bitsPerQuarter;
    uint32_t bitsSoFar;
    uint8_t busloadPercentage;
} BUSLOAD;

// class CAN_COMMON;
class CAN_FRAME;

class CANManager
{
public:
    CANManager();
    void sendFrame(CAN_FRAME &frame);
    void displayFrame(CAN_FRAME &frame, int whichBus);
    void loop();
    void setup();

private:
    BUSLOAD busLoad[NUM_BUSES];
    uint32_t busLoadTimer;
};

void canRxTask(void *arg);
void transportTask(void *arg);

extern volatile bool canPauseRX;

