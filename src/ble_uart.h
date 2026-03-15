#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

class BLEUART : public NimBLECharacteristicCallbacks
{
public:
    BLEUART();

    void begin(const char *deviceName);

    int available();
    int read();
    void write(const uint8_t *data, size_t len);

protected:
    void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;

private:
    NimBLEServer *server;
    NimBLEService *service;
    NimBLECharacteristic *txChar;
    NimBLECharacteristic *rxChar;

    static const int RX_BUF_SIZE = 256;

    uint8_t rxBuf[RX_BUF_SIZE];
    volatile uint16_t rxHead;
    volatile uint16_t rxTail;
};

class BLEServerCallbacksImpl : public NimBLEServerCallbacks
{
public:
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
};