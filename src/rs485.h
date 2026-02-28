#pragma once
#include <Arduino.h>

class RS485Port
{
public:
    void begin(uint32_t baud);
    void print(const char *str);
    void println(const char *str);
    void printf(const char *format, ...);
    void write(const uint8_t *data, size_t len);

private:
    void setTX();
    void setRX();
};

extern RS485Port RS485;