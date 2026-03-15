#include "ble_uart.h"
#include "config.h"

#define UART_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_RX_CHAR_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define UART_TX_CHAR_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEUART::BLEUART()
{
    rxHead = 0;
    rxTail = 0;
}

void BLEUART::begin(const char *deviceName)
{
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(247);
    server = NimBLEDevice::createServer();
    server->setCallbacks(new BLEServerCallbacksImpl());

    server = NimBLEDevice::createServer();

    service = server->createService(UART_SERVICE_UUID);

    txChar = service->createCharacteristic(
        UART_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY);

    rxChar = service->createCharacteristic(
        UART_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

    rxChar->setCallbacks(this);

    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();

    NimBLEAdvertisementData advData;
    NimBLEAdvertisementData scanResp;

    advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    advData.setName(deviceName);

    scanResp.addServiceUUID(UART_SERVICE_UUID);

    advertising->setAdvertisementData(advData);
    advertising->setScanResponseData(scanResp);

    advertising->start();

    DEBUGLN("BLE advertising started");
}

void BLEUART::onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo)
{
    std::string val = pCharacteristic->getValue();

    for (size_t i = 0; i < val.length(); i++)
    {
        uint16_t next = (rxHead + 1) & (RX_BUF_SIZE - 1);

        if (next != rxTail)
        {
            rxBuf[rxHead] = val[i];
            rxHead = next;
        }
    }
}

int BLEUART::available()
{
    return rxHead != rxTail;
}

int BLEUART::read()
{
    if (rxHead == rxTail)
        return -1;

    uint8_t v = rxBuf[rxTail];
    rxTail = (rxTail + 1) & (RX_BUF_SIZE - 1);

    return v;
}

void BLEUART::write(const uint8_t *data, size_t len)
{
    if(server->getConnectedCount())
    {
        txChar->setValue(data, len);
        txChar->notify();
    }
}

void BLEServerCallbacksImpl::onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo)
{
    DEBUGLN("BLE client connected");
}

void BLEServerCallbacksImpl::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason)
{
    DEBUGLN("BLE client disconnected");

    NimBLEDevice::getAdvertising()->start();
}