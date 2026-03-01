#include "rs485.h"
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#define RS485_DE_PIN 17
#define RS485_RO_PIN 21
#define RS485_DI_PIN 22

static HardwareSerial RS485Serial(2);

extern SemaphoreHandle_t serialMutex;

RS485Port RS485;

void RS485Port::begin(uint32_t baud)
{
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW); // start in RX

    RS485Serial.begin(baud, SERIAL_8N1, RS485_RO_PIN, RS485_DI_PIN);
}

void RS485Port::setTX()
{
    digitalWrite(RS485_DE_PIN, HIGH);
}

void RS485Port::setRX()
{
    digitalWrite(RS485_DE_PIN, LOW);
}

void RS485Port::print(const char *str)
{
    if (serialMutex)
        xSemaphoreTake(serialMutex, portMAX_DELAY);

    setTX();
    RS485Serial.print(str);
    RS485Serial.flush();
    setRX();

    if (serialMutex)
        xSemaphoreGive(serialMutex);
}

void RS485Port::println(const char *str)
{
    if (serialMutex)
        xSemaphoreTake(serialMutex, portMAX_DELAY);

    setTX();
    RS485Serial.println(str);
    RS485Serial.flush();
    setRX();

    if (serialMutex)
        xSemaphoreGive(serialMutex);
}

void RS485Port::printf(const char *format, ...)
{
    char buffer[256];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (serialMutex)
        xSemaphoreTake(serialMutex, portMAX_DELAY);

    setTX();
    RS485Serial.print(buffer);
    RS485Serial.flush();
    setRX();

    if (serialMutex)
        xSemaphoreGive(serialMutex);
}

void RS485Port::write(const uint8_t *data, size_t len)
{
    if (serialMutex)
        xSemaphoreTake(serialMutex, portMAX_DELAY);

    setTX();
    RS485Serial.write(data, len);
    RS485Serial.flush();
    setRX();

    if (serialMutex)
        xSemaphoreGive(serialMutex);
}