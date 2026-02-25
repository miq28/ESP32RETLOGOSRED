#include "commbuffer.h"
#include "Logger.h"
#include "gvret_comm.h"

CommBuffer::CommBuffer()
{
    transmitBufferLength = 0;
}

size_t CommBuffer::numAvailableBytes()
{
    return transmitBufferLength;
}

void CommBuffer::clearBufferedBytes()
{
    transmitBufferLength = 0;
}

uint8_t *CommBuffer::getBufferedBytes()
{
    return transmitBuffer;
}

// ----------------------------------------------------
// Safe append helpers
// ----------------------------------------------------

static inline bool hasSpace(size_t current, size_t add)
{
    return (current + add) <= WIFI_BUFF_SIZE;
}

void CommBuffer::sendBytesToBuffer(uint8_t *bytes, size_t length)
{
    if (!hasSpace(transmitBufferLength, length))
        return;

    memcpy(&transmitBuffer[transmitBufferLength], bytes, length);
    transmitBufferLength += length;
}

void CommBuffer::sendByteToBuffer(uint8_t byt)
{
    if (!hasSpace(transmitBufferLength, 1))
        return;

    transmitBuffer[transmitBufferLength++] = byt;
}

void CommBuffer::sendString(String str)
{
    sendCharString((char *)str.c_str());
}

void CommBuffer::sendCharString(char *str)
{
    size_t len = strlen(str);
    if (!hasSpace(transmitBufferLength, len))
        return;

    memcpy(&transmitBuffer[transmitBufferLength], str, len);
    transmitBufferLength += len;

    Logger::debug("Queued %u bytes", len);
}

// ----------------------------------------------------
// CAN CLASSIC
// ----------------------------------------------------

void CommBuffer::sendFrameToBuffer(CAN_FRAME &frame, int whichBus)
{
    uint32_t now = micros();
    uint32_t id = frame.id;

    if (settings.useBinarySerialComm)
    {
        if (frame.extended)
            id |= (1UL << 31);

        size_t required = 1 + 1 + 4 + 4 + 1 + frame.length + 1;

        while (!hasSpace(transmitBufferLength, required))
        {
            // Wait until main loop flushes buffer
            delayMicroseconds(50);
        }

        uint8_t *p = &transmitBuffer[transmitBufferLength];

        *p++ = 0xF1;
        *p++ = 0;

        memcpy(p, &now, 4);
        p += 4;
        memcpy(p, &id, 4);
        p += 4;

        *p++ = frame.length + (whichBus << 4);

        memcpy(p, frame.data.uint8, frame.length);
        p += frame.length;

        *p++ = 0;

        transmitBufferLength += required;
    }
    else
    {
        int written = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            WIFI_BUFF_SIZE - transmitBufferLength,
            "%lu - %lx %c %d %d",
            now,
            id,
            frame.extended ? 'X' : 'S',
            whichBus,
            frame.length);

        if (written <= 0)
            return;
        transmitBufferLength += written;

        for (int i = 0; i < frame.length; i++)
        {
            written = snprintf(
                (char *)&transmitBuffer[transmitBufferLength],
                WIFI_BUFF_SIZE - transmitBufferLength,
                " %x",
                frame.data.uint8[i]);

            if (written <= 0)
                return;
            transmitBufferLength += written;
        }

        if (hasSpace(transmitBufferLength, 2))
        {
            transmitBuffer[transmitBufferLength++] = '\r';
            transmitBuffer[transmitBufferLength++] = '\n';
        }
    }
}

// ----------------------------------------------------
// CAN FD
// ----------------------------------------------------

void CommBuffer::sendFrameToBuffer(CAN_FRAME_FD &frame, int whichBus)
{
    uint32_t now = micros();
    uint32_t id = frame.id;

    if (settings.useBinarySerialComm)
    {
        if (frame.extended)
            id |= (1UL << 31);

        size_t required = 1 + 1 + 4 + 4 + 1 + 1 + frame.length + 1;

        while (!hasSpace(transmitBufferLength, required))
        {
            // Wait until main loop flushes buffer
            delayMicroseconds(50);
        }

        uint8_t *p = &transmitBuffer[transmitBufferLength];

        *p++ = 0xF1;
        *p++ = PROTO_BUILD_FD_FRAME;

        memcpy(p, &now, 4);
        p += 4;
        memcpy(p, &id, 4);
        p += 4;

        *p++ = frame.length;
        *p++ = (uint8_t)whichBus;

        memcpy(p, frame.data.uint8, frame.length);
        p += frame.length;

        *p++ = 0;

        transmitBufferLength += required;
    }
    else
    {
        int written = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            WIFI_BUFF_SIZE - transmitBufferLength,
            "%lu - %lx %c %d %d",
            now,
            id,
            frame.extended ? 'X' : 'S',
            whichBus,
            frame.length);

        if (written <= 0)
            return;
        transmitBufferLength += written;

        for (int i = 0; i < frame.length; i++)
        {
            written = snprintf(
                (char *)&transmitBuffer[transmitBufferLength],
                WIFI_BUFF_SIZE - transmitBufferLength,
                " %x",
                frame.data.uint8[i]);

            if (written <= 0)
                return;
            transmitBufferLength += written;
        }

        if (hasSpace(transmitBufferLength, 2))
        {
            transmitBuffer[transmitBufferLength++] = '\r';
            transmitBuffer[transmitBufferLength++] = '\n';
        }
    }
}