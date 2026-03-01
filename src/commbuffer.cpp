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

uint8_t* CommBuffer::getBufferedBytes()
{
    return transmitBuffer;
}

//a bit faster version that blasts through the copy more efficiently.
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

// void CommBuffer::sendFrameToBuffer(CAN_FRAME &frame, int whichBus)
// {
//     uint8_t temp;
//     size_t writtenBytes;
//     if (settings.useBinarySerialComm) {
//         if (frame.extended) frame.id |= 1 << 31;
//         transmitBuffer[transmitBufferLength++] = 0xF1;
//         transmitBuffer[transmitBufferLength++] = 0; //0 = canbus frame sending
//         uint32_t now = micros();
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now & 0xFF);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id & 0xFF);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 8);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 16);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 24);
//         transmitBuffer[transmitBufferLength++] = frame.length + (uint8_t)(whichBus << 4);
//         for (int c = 0; c < frame.length; c++) {
//             transmitBuffer[transmitBufferLength++] = frame.data.uint8[c];
//         }
//         //temp = checksumCalc(buff, 11 + frame.length);
//         temp = 0;
//         transmitBuffer[transmitBufferLength++] = temp;
//         //Serial.write(buff, 12 + frame.length);
//     } else {
//         writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], "%d - %x", micros(), frame.id);
//         transmitBufferLength += writtenBytes;
//         if (frame.extended) sprintf((char *)&transmitBuffer[transmitBufferLength], " X ");
//         else sprintf((char *)&transmitBuffer[transmitBufferLength], " S ");
//         transmitBufferLength += 3;
//         writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], "%i %i", whichBus, frame.length);
//         transmitBufferLength += writtenBytes;
//         for (int c = 0; c < frame.length; c++) {
//             writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], " %x", frame.data.uint8[c]);
//             transmitBufferLength += writtenBytes;
//         }
//         sprintf((char *)&transmitBuffer[transmitBufferLength], "\r\n");
//         transmitBufferLength += 2;
//     }
// }

// void CommBuffer::sendFrameToBuffer(CAN_FRAME_FD &frame, int whichBus)
// {
//     uint8_t temp;
//     size_t writtenBytes;
//     if (settings.useBinarySerialComm) {
//         if (frame.extended) frame.id |= 1 << 31;
//         transmitBuffer[transmitBufferLength++] = 0xF1;
//         transmitBuffer[transmitBufferLength++] = PROTO_BUILD_FD_FRAME;
//         uint32_t now = micros();
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now & 0xFF);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id & 0xFF);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 8);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 16);
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 24);
//         transmitBuffer[transmitBufferLength++] = frame.length;
//         transmitBuffer[transmitBufferLength++] = (uint8_t)(whichBus);
//         for (int c = 0; c < frame.length; c++) {
//             transmitBuffer[transmitBufferLength++] = frame.data.uint8[c];
//         }
//         //temp = checksumCalc(buff, 11 + frame.length);
//         temp = 0;
//         transmitBuffer[transmitBufferLength++] = temp;
//         //Serial.write(buff, 12 + frame.length);
//     } else {
//         writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], "%d - %x", micros(), frame.id);
//         transmitBufferLength += writtenBytes;
//         if (frame.extended) sprintf((char *)&transmitBuffer[transmitBufferLength], " X ");
//         else sprintf((char *)&transmitBuffer[transmitBufferLength], " S ");
//         transmitBufferLength += 3;
//         writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], "%i %i", whichBus, frame.length);
//         transmitBufferLength += writtenBytes;
//         for (int c = 0; c < frame.length; c++) {
//             writtenBytes = sprintf((char *)&transmitBuffer[transmitBufferLength], " %x", frame.data.uint8[c]);
//             transmitBufferLength += writtenBytes;
//         }
//         sprintf((char *)&transmitBuffer[transmitBufferLength], "\r\n");
//         transmitBufferLength += 2;
//     }
// }



// revised
void CommBuffer::sendFrameToBuffer(CAN_FRAME &frame, int whichBus)
{
    uint8_t temp;
    size_t writtenBytes;

    if (settings.useBinarySerialComm)
    {
        size_t needed = 12 + frame.length;  // rough size

        if (transmitBufferLength + needed > WIFI_BUFF_SIZE)
            return; // drop if not enough space

        if (frame.extended) frame.id |= 1 << 31;

        transmitBuffer[transmitBufferLength++] = 0xF1;
        transmitBuffer[transmitBufferLength++] = 0;

        uint32_t now = micros();

        transmitBuffer[transmitBufferLength++] = (uint8_t)(now);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);

        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 24);

        transmitBuffer[transmitBufferLength++] =
            frame.length + (uint8_t)(whichBus << 4);

        for (int c = 0; c < frame.length; c++)
            transmitBuffer[transmitBufferLength++] = frame.data.uint8[c];

        temp = 0;
        transmitBuffer[transmitBufferLength++] = temp;
    }
    else
    {
        size_t remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining == 0) return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%lu - %lx",
            micros(),
            frame.id);

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining < 4) return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            frame.extended ? " X " : " S ");

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%d %d",
            whichBus,
            frame.length);

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        for (int c = 0; c < frame.length; c++)
        {
            remaining = WIFI_BUFF_SIZE - transmitBufferLength;
            if (remaining == 0) return;

            writtenBytes = snprintf(
                (char *)&transmitBuffer[transmitBufferLength],
                remaining,
                " %x",
                frame.data.uint8[c]);

            if (writtenBytes >= remaining) return;
            transmitBufferLength += writtenBytes;
        }

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining < 3) return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "\r\n");

        if (writtenBytes < remaining)
            transmitBufferLength += writtenBytes;
    }
}

void CommBuffer::sendFrameToBuffer(CAN_FRAME_FD &frame, int whichBus)
{
    uint8_t temp;
    size_t writtenBytes;

    if (settings.useBinarySerialComm)
    {
        size_t needed = 13 + frame.length;

        if (transmitBufferLength + needed > WIFI_BUFF_SIZE)
            return;

        if (frame.extended) frame.id |= 1 << 31;

        transmitBuffer[transmitBufferLength++] = 0xF1;
        transmitBuffer[transmitBufferLength++] = PROTO_BUILD_FD_FRAME;

        uint32_t now = micros();

        transmitBuffer[transmitBufferLength++] = (uint8_t)(now);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(now >> 24);

        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 8);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 16);
        transmitBuffer[transmitBufferLength++] = (uint8_t)(frame.id >> 24);

        transmitBuffer[transmitBufferLength++] = frame.length;
        transmitBuffer[transmitBufferLength++] = (uint8_t)(whichBus);

        for (int c = 0; c < frame.length; c++)
            transmitBuffer[transmitBufferLength++] = frame.data.uint8[c];

        temp = 0;
        transmitBuffer[transmitBufferLength++] = temp;
    }
    else
    {
        size_t remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining == 0) return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%lu - %lx",
            micros(),
            frame.id);

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            frame.extended ? " X " : " S ");

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "%d %d",
            whichBus,
            frame.length);

        if (writtenBytes >= remaining) return;
        transmitBufferLength += writtenBytes;

        for (int c = 0; c < frame.length; c++)
        {
            remaining = WIFI_BUFF_SIZE - transmitBufferLength;
            if (remaining == 0) return;

            writtenBytes = snprintf(
                (char *)&transmitBuffer[transmitBufferLength],
                remaining,
                " %x",
                frame.data.uint8[c]);

            if (writtenBytes >= remaining) return;
            transmitBufferLength += writtenBytes;
        }

        remaining = WIFI_BUFF_SIZE - transmitBufferLength;
        if (remaining < 3) return;

        writtenBytes = snprintf(
            (char *)&transmitBuffer[transmitBufferLength],
            remaining,
            "\r\n");

        if (writtenBytes < remaining)
            transmitBufferLength += writtenBytes;
    }
}

