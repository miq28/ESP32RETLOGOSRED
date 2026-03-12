#pragma once
#include <stdint.h>

class LAWICELHandler
{
public:
    void handleLongCmd(char *buffer);
    void handleShortCmd(char cmd);
    void sendFrameToBuffer(uint32_t id, bool extended, uint8_t length, uint8_t *data, int whichBus);

private:
    char tokens[14][10];

    void tokenizeCmdString(char *buff);
    void uppercaseToken(char *token);
    void printBusName(int bus);
};