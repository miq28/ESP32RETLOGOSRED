#pragma once
#include <stdint.h>

struct CAN_FRAME
{
    uint32_t id;        // 11-bit or 29-bit identifier
    uint32_t timestamp; // microseconds when received

    uint8_t length;     // DLC 0-8
    uint8_t extended;   // 1 = 29-bit ID
    uint8_t rtr;        // 1 = remote frame
    uint8_t bus;        // bus index

    uint8_t data[8];    // payload
};