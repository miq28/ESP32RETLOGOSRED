/*
Implements the lawicel protocol.
*/

#include "lawicel.h"
#include "config.h"
#include "can_driver.h"
#include "can_frame.h"
#include "utility.h"

void LAWICELHandler::handleShortCmd(char cmd)
{
    switch (cmd)
    {
    case 'O': // LAWICEL open canbus port
        can_init(settings.canSettings[0].nomSpeed);
        Serial.write(13); // send CR to mean "ok"
        SysSettings.lawicelMode = true;
        break;
    case 'C': // LAWICEL close canbus port (First one)
        can_stop();
        Serial.write(13); // send CR to mean "ok"
        break;
    case 'L':                                       // LAWICEL open canbus port in listen only mode
        can_init(settings.canSettings[0].nomSpeed); // listen-only not yet supported
        Serial.write(13);                           // send CR to mean "ok"
        SysSettings.lawicelMode = true;
        break;
    case 'P': // LAWICEL - poll for one waiting frame. Or, just CR if no frames
        Serial.write(13);
        break;
    case 'A': // LAWICEL - poll for all waiting frames - CR if no frames
        Serial.write(13);
        break;
    case 'F':                // LAWICEL - read status bits
        Serial.print("F00"); // bit 0 = RX Fifo Full, 1 = TX Fifo Full, 2 = Error warning, 3 = Data overrun, 5= Error passive, 6 = Arb. Lost, 7 = Bus Error
        Serial.write(13);
        break;
    case 'V': // LAWICEL - get version number
        Serial.print("V1013\n");
        SysSettings.lawicelMode = true;
        break;
    case 'N': // LAWICEL - get serial number
        Serial.print("ESP32RET\n");
        SysSettings.lawicelMode = true;
        break;
    case 'x':
        SysSettings.lawicellExtendedMode = !SysSettings.lawicellExtendedMode;
        if (SysSettings.lawicellExtendedMode)
        {
            Serial.print("V2\n");
        }
        else
        {
            Serial.print("LAWICEL\n");
        }
        break;
    case 'B': // LAWICEL V2 - Output list of supported buses
        if (SysSettings.lawicellExtendedMode)
        {
            for (int i = 0; i < NUM_BUSES; i++)
            {
                printBusName(i);
                Serial.print("\n");
            }
        }
        break;
    case 'X':
        if (SysSettings.lawicellExtendedMode)
        {
        }
        break;
    }
}

void LAWICELHandler::handleLongCmd(char *buffer)
{
    char buff[80];
    int val;

    tokenizeCmdString(buffer);

    switch (buffer[0])
    {
    case 't': // transmit standard frame
    {
        uint32_t id = Utility::parseHexString(buffer + 1, 3);
        uint8_t len = buffer[4] - '0';

        if (len > 8)
            len = 8;

        uint8_t data[8];

        for (int i = 0; i < len; i++)
        {
            data[i] = Utility::parseHexString(buffer + 5 + (2 * i), 2);
        }

        can_send(id, false, false, len, data);
    }
    break;
    case 'T': // transmit extended frame
    {
        uint32_t id = Utility::parseHexString(buffer + 1, 8);
        uint8_t len = buffer[9] - '0';

        if (len > 8)
            len = 8;

        uint8_t data[8];

        for (int i = 0; i < len; i++)
        {
            data[i] = Utility::parseHexString(buffer + 10 + (2 * i), 2);
        }

        can_send(id, true, false, len, data);
    }
    break;
    case 'S':
        if (!SysSettings.lawicellExtendedMode)
        {
            // setup canbus baud via predefined speeds
            val = Utility::parseHexCharacter(buffer[1]);
            switch (val)
            {
            case 0:
                settings.canSettings[0].nomSpeed = 10000;
                break;
            case 1:
                settings.canSettings[0].nomSpeed = 20000;
                break;
            case 2:
                settings.canSettings[0].nomSpeed = 50000;
                break;
            case 3:
                settings.canSettings[0].nomSpeed = 100000;
                break;
            case 4:
                settings.canSettings[0].nomSpeed = 125000;
                break;
            case 5:
                settings.canSettings[0].nomSpeed = 250000;
                break;
            case 6:
                settings.canSettings[0].nomSpeed = 500000;
                break;
            case 7:
                settings.canSettings[0].nomSpeed = 800000;
                break;
            case 8:
                settings.canSettings[0].nomSpeed = 1000000;
                break;
            }
        }
        else
        { // LAWICEL V2 - Send packet out of specified bus - S <Bus> <ID> <Data0> <Data1> <...>
            uint8_t bytes[8];
            uint32_t id;
            int numBytes = 0;
            id = strtol(tokens[2], nullptr, 16);
            for (int b = 0; b < 8; b++)
            {
                if (tokens[3 + b][0] != 0)
                {
                    bytes[b] = strtol(tokens[3 + b], nullptr, 16);
                    numBytes++;
                }
                else
                    break; // break for loop because we're obviously done.
            }
            if (!strcasecmp(tokens[1], "CAN0"))
            {
                can_send(id, false, false, numBytes, bytes);
            }
        }
    case 's': // setup canbus baud via register writes (we can't really do that...)
        // settings.CAN0Speed = 250000;
        break;
    case 'r': // send a standard RTR frame (don't really... that's so deprecated its not even funny)
        break;
    case 'R':
        if (SysSettings.lawicellExtendedMode)
        { // Lawicel V2 - Set that we want to receive traffic from the given bus - R <BUSID>
            if (!strcasecmp(tokens[1], "CAN0"))
                SysSettings.lawicelBusReception[0] = true;
            if (!strcasecmp(tokens[1], "CAN1"))
                SysSettings.lawicelBusReception[1] = true;
        }
        else
        { // Lawicel V1 - send extended RTR frame (NO! DON'T DO IT!)
        }
        break;
    case 'X': // Set autopoll off/on
        if (buffer[1] == '1')
            SysSettings.lawicelAutoPoll = true;
        else
            SysSettings.lawicelAutoPoll = false;
        break;
    case 'W':  // Dual or single filter mode
        break; // don't actually support this mode
    case 'm':  // set acceptance mask - these things seem to be odd and aren't actually implemented yet
    case 'M':
        if (SysSettings.lawicellExtendedMode)
        { // Lawicel V2 - Set filter mask - M <busid> <Mask> <FilterID> <Ext?>
            int mask = strtol(tokens[2], nullptr, 16);
            int filt = strtol(tokens[3], nullptr, 16);
            if (!strcasecmp(tokens[1], "CAN0"))
            {
                if (!strcasecmp(tokens[4], "X"))
                {
                    // filter configuration not implemented in TWAI driver layer
                }
                else
                {
                    // filter configuration not implemented in TWAI driver layer
                }
            }
        }
        else
        { // Lawicel V1 - set acceptance code
        }
        break;
    case 'H':
        if (SysSettings.lawicellExtendedMode)
        { // Lawicel V2 - Halt reception of traffic from given bus - H <busid>
            if (!strcasecmp(tokens[1], "CAN0"))
                SysSettings.lawicelBusReception[0] = false;
            if (!strcasecmp(tokens[1], "CAN1"))
                SysSettings.lawicelBusReception[1] = false;
        }
        break;
    case 'U':  // set uart speed. We just ignore this. You can't set a baud rate on a USB CDC port
        break; // also no action here
    case 'Z':  // Turn timestamp off/on
        if (buffer[1] == '1')
            SysSettings.lawicelTimestamping = true;
        else
            SysSettings.lawicelTimestamping = false;
        break;
    case 'Q':  // turn auto start up on/off - probably don't need to actually implement this at the moment.
        break; // no action yet or maybe ever
    case 'C':  // Lawicel V2 - configure one of the buses - C <busid> <speed> <any additional needed params>
        if (SysSettings.lawicellExtendedMode)
        {
            // at least two parameters separated by spaces. First BUS ID (CAN0, CAN1, SWCAN, etc) then speed (or more params separated by #'s)
            int speed = atoi(tokens[2]);
            if (!strcasecmp(tokens[1], "CAN0"))
            {
                can_set_speed(speed);
            }
        }
        break;
    }
    Serial.write(13);
}

// Tokenize cmdBuffer on space boundaries - up to 10 tokens supported
void LAWICELHandler::tokenizeCmdString(char *buff)
{
    int idx = 0;
    char *tok;

    for (int i = 0; i < 13; i++)
        tokens[i][0] = 0;

    tok = strtok(buff, " ");
    if (tok != nullptr)
        strcpy(tokens[idx], tok);
    else
        tokens[idx][0] = 0;
    while (tokens[idx] != nullptr && idx < 13)
    {
        idx++;
        tok = strtok(nullptr, " ");
        if (tok != nullptr)
            strcpy(tokens[idx], tok);
        else
            tokens[idx][0] = 0;
    }
}

void LAWICELHandler::uppercaseToken(char *token)
{
    int idx = 0;
    while (token[idx] != 0 && idx < 9)
    {
        token[idx] = toupper(token[idx]);
        idx++;
    }
    token[idx] = 0;
}

void LAWICELHandler::printBusName(int bus)
{
    switch (bus)
    {
    case 0:
        Serial.print("CAN0");
        break;
    case 1:
        Serial.print("CAN1");
        break;
    default:
        Serial.print("UNKNOWN");
        break;
    }
}

void LAWICELHandler::sendFrameToBuffer(uint32_t id, bool extended, uint8_t length, uint8_t *data, int whichBus)
{
    uint8_t buff[40];
    uint8_t writtenBytes;
    uint8_t temp;
    uint32_t now = micros();

    if (SysSettings.lawicellExtendedMode)
    {
        Serial.print(micros());
        Serial.print(" - ");
        Serial.print(id, HEX);
        if (extended)
            Serial.print(" X ");
        // serialEnqueueFormat(" X ");
        else
            Serial.print(" S ");

        printBusName(whichBus);
        for (int d = 0; d < length; d++)
        {
            Serial.print(" ");
            Serial.print(data[d], HEX);
        }
    }
    else
    {
        if (extended)
        {
            Serial.print("T");
            sprintf((char *)buff, "%08x", id);
            Serial.print((char *)buff);
        }
        else
        {
            Serial.print("t");
            sprintf((char *)buff, "%03x", id);
            Serial.print((char *)buff);
        }
        Serial.print(length);
        for (int i = 0; i < length; i++)
        {
            sprintf((char *)buff, "%02x", data[i]);
            Serial.print((char *)buff);
        }
        if (SysSettings.lawicelTimestamping)
        {
            uint16_t timestamp = (uint16_t)millis();
            sprintf((char *)buff, "%04x", timestamp);
            Serial.print((char *)buff);
        }
    }
    Serial.write(13);
}