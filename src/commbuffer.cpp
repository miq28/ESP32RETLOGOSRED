#include "commbuffer.h"
#include "Logger.h"
#include "gvret_comm.h"

volatile uint32_t gvretDroppedFrames = 0;

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

// a bit faster version that blasts through the copy more efficiently.
void CommBuffer::sendBytesToBuffer(uint8_t *bytes, size_t length)
{
    if (transmitBufferLength + length <= WIFI_BUFF_SIZE)
    {
        memcpy(&transmitBuffer[transmitBufferLength], bytes, length);
        transmitBufferLength += length;
    }
}

void CommBuffer::sendByteToBuffer(uint8_t byt)
{
    if (transmitBufferLength < WIFI_BUFF_SIZE)
    {
        transmitBuffer[transmitBufferLength++] = byt;
    }
}
void CommBuffer::sendString(String str)
{
    char buff[300];
    str.toCharArray(buff, 300);
    sendCharString(buff);
}

void CommBuffer::sendCharString(char *str)
{
    char *p = str;
    int i = 0;
    while (*p)
    {
        sendByteToBuffer(*p++);
        i++;
    }
    Logger::debug("Queued %i bytes", i);
}

// revised
void CommBuffer::sendFrameToBuffer(CAN_FRAME &frame, int whichBus)
{
    uint8_t temp;
    size_t writtenBytes;

    if (settings.useBinarySerialComm)
    {
        uint8_t dlc = frame.length & 0x0F;
        if (dlc > 8)
            dlc = 8;

        size_t frameSize = 12 + dlc;

        if (transmitBufferLength + frameSize >= WIFI_BUFF_SIZE)
        {
            gvretDroppedFrames++;
            return; // drop frame instead of corrupting stream
        }

        transmitBuffer[transmitBufferLength++] = 0xF1;
        transmitBuffer[transmitBufferLength++] = 0;

        uint32_t now = micros();

        transmitBuffer[transmitBufferLength++] = (uint8_t)now;
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);

        uint32_t id = frame.id;
        if (frame.extended)
            id |= 0x80000000;

        transmitBuffer[transmitBufferLength++] = (uint8_t)id;
        transmitBuffer[transmitBufferLength++] = (uint8_t)(id >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(id >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(id >> 24);

        transmitBuffer[transmitBufferLength++] =
            (dlc & 0x0F) | ((whichBus & 0x0F) << 4);

        for (int c = 0; c < dlc; c++)
            transmitBuffer[transmitBufferLength++] = frame.data[c];

        temp = 0;
        transmitBuffer[transmitBufferLength++] = temp;
    }
    else
    {
        size_t remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining == 0)
            return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%lu - %lx",
            micros(),
            frame.id);

        if (writtenBytes >= remaining)
            return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining < 4)
            return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            frame.extended ? " X " : " S ");

        if (writtenBytes >= remaining)
            return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%d %d",
            whichBus,
            frame.length);

        if (writtenBytes >= remaining)
            return;
        transmitBufferLength += writtenBytes;

        for (int c = 0; c < frame.length; c++)
        {
            remaining = WIFI_BUFF_SIZE - transmitBufferLength;
            if (remaining == 0)
                return;

            writtenBytes = snprintf(
                (char *)&transmitBuffer[transmitBufferLength],
                remaining,
                " %x",
                frame.data[c]);

            if (writtenBytes >= remaining)
                return;
            transmitBufferLength += writtenBytes;
        }

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining < 3)
            return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "\r\n");

        if (writtenBytes < remaining)
            transmitBufferLength += writtenBytes;
    }
}
