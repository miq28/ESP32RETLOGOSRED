/*
 ESP32RET.ino

 Created: June 1, 2020
 Author: Collin Kidder

Copyright (c) 2014-2020 Collin Kidder, Michael Neuweiler

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include <esp32_can.h>
#include <SPI.h>
#include <Preferences.h>
#include "ELM327_Emulator.h"
#include "SerialConsole.h"
#include "wifi_manager.h"
#include "gvret_comm.h"
#include "can_manager.h"
#include "lawicel.h"

#include "USB.h"

byte i = 0;

uint32_t lastFlushMicros = 0;

bool markToggle[6];
uint32_t lastMarkTrigger = 0;

EEPROMSettings settings;
SystemSettings SysSettings;
Preferences nvPrefs;
char otaHost[40];
char otaFilename[100];
char deviceName[20];

uint8_t espChipRevision;

ELM327Emu elmEmulator;

WiFiManager wifiManager;

GVRET_Comm_Handler serialGVRET; // gvret protocol over the serial to USB connection
GVRET_Comm_Handler wifiGVRET;   // GVRET over the wifi telnet port
CANManager canManager;          // keeps track of bus load and abstracts away some details of how things are done
LAWICELHandler lawicel;

SerialConsole console;

CAN_COMMON *canBuses[NUM_BUSES];

// initializes all the system EEPROM values. Chances are this should be broken out a bit but
// there is only one checksum check for all of them so it's simple to do it all here.
void loadSettings()
{
    Logger::console("Loading settings....");

    for (int i = 0; i < NUM_BUSES; i++)
        canBuses[i] = nullptr;

    nvPrefs.begin(PREF_NAME, false);

    settings.useBinarySerialComm = nvPrefs.getBool("binarycomm", false);
    settings.logLevel = nvPrefs.getUChar("loglevel", 1); // info
    settings.wifiMode = nvPrefs.getUChar("wifiMode", 1); // Wifi defaults to creating an AP
    settings.enableBT = nvPrefs.getBool("enable-bt", false);
    settings.enableLawicel = nvPrefs.getBool("enableLawicel", true);
    settings.systemType = 0;

    if (settings.systemType == 0)
    {
        canBuses[0] = &CAN0;
        SysSettings.logToggle = true;
        SysSettings.txToggle = true;
        SysSettings.rxToggle = true;
        SysSettings.lawicelAutoPoll = false;
        SysSettings.lawicelMode = false;
        SysSettings.lawicellExtendedMode = false;
        SysSettings.lawicelTimestamping = false;
        SysSettings.numBuses = 1;
        SysSettings.isWifiActive = false;
        SysSettings.isWifiConnected = false;
        strcpy(otaHost, "");
        strcpy(otaFilename, "");
#ifdef CONFIG_IDF_TARGET_ESP32S3
        CAN0.setCANPins(GPIO_NUM_4, GPIO_NUM_5);
#else
        CAN0.setCANPins(GPIO_NUM_26, GPIO_NUM_27);
#endif

    }

    if (nvPrefs.getString("SSID", settings.SSID, 32) == 0)
    {
        if (settings.wifiMode == 1)
        {
            strcpy(settings.SSID, "galaxi");
        }
        else
        {
            strcpy(settings.SSID, "NeedForStoicism");
        }
    }

    if (nvPrefs.getString("wpa2Key", settings.WPA2Key, 64) == 0)
    {
        if (settings.wifiMode == 1)
        {
            strcpy(settings.WPA2Key, "n1n4iqb4l");
        }
        else
        {
            strcpy(settings.WPA2Key, "by.virtue.defended");
        }
    }

    if (nvPrefs.getString("btname", settings.btName, 32) == 0)
    {
        strcpy(settings.btName, "ELM327-NeedForStoicism");
    }

    if (nvPrefs.getString("deviceName", deviceName, 20) == 0)
    {
        strcpy(deviceName, "ESP32RET");
    }

    char buff[80];
    for (int i = 0; i < SysSettings.numBuses; i++)
    {
        sprintf(buff, "can%ispeed", i);
        settings.canSettings[i].nomSpeed = nvPrefs.getUInt(buff, 500000);
        sprintf(buff, "can%i_en", i);
        settings.canSettings[i].enabled = nvPrefs.getBool(buff, (i < 2) ? true : false);
        sprintf(buff, "can%i-listenonly", i);
        settings.canSettings[i].listenOnly = nvPrefs.getBool(buff, false);
        sprintf(buff, "can%i-fdspeed", i);
        settings.canSettings[i].fdSpeed = nvPrefs.getUInt(buff, 5000000);
        sprintf(buff, "can%i-fdmode", i);
        settings.canSettings[i].fdMode = nvPrefs.getBool(buff, false);
    }

    nvPrefs.end();

    Logger::setLoglevel((Logger::LogLevel)settings.logLevel);

    for (int rx = 0; rx < NUM_BUSES; rx++)
        SysSettings.lawicelBusReception[rx] = true; // default to showing messages on RX
}

void setup()
{
    espChipRevision = ESP.getChipRevision();

#ifdef CONFIG_IDF_TARGET_ESP32S3
    // Serial.begin(1000000);
    Serial.begin(115200);
    DEBUGPORT.begin(115200);
#else
    DEBUGPORT.begin(115200);
#endif

    SysSettings.isWifiConnected = false;

    loadSettings();

    if (settings.enableBT)
    {
        Serial.println("Starting Bluetooth");
        elmEmulator.setup();
    }
    wifiManager.setup();

    Serial.println("");
    Serial.println("=====================================");
    Serial.println("     Hacking Cars with Logos Red     ");
    Serial.println("                                     ");
    Serial.println("      Choose not to be harmed        ");
    Serial.println("      and you won't be harmed        ");
    Serial.println("                                     ");
    Serial.println("        - Marcus Aurelius            ");
    Serial.println("=====================================");
    Serial.println("");

    canManager.setup();

    SysSettings.lawicelMode = false;
    SysSettings.lawicelAutoPoll = false;
    SysSettings.lawicelTimestamping = false;
    SysSettings.lawicelPollCounter = 0;
}

void sendMarkTriggered(int which)
{
    CAN_FRAME frame;
    frame.id = 0xFFFFFFF8ull + which;
    frame.extended = true;
    frame.length = 0;
    frame.rtr = 0;
    canManager.displayFrame(frame, 0);
}

void loop()
{
    bool isConnected = false;
    int serialCnt;
    uint8_t in_byte;

    isConnected = true;

    if (SysSettings.lawicelPollCounter > 0)
        SysSettings.lawicelPollCounter--;

    canManager.loop();
    wifiManager.loop();

    size_t wifiLength = wifiGVRET.numAvailableBytes();
    size_t serialLength = serialGVRET.numAvailableBytes();
    size_t maxLength = (wifiLength > serialLength) ? wifiLength : serialLength;

    if ((micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL) || (maxLength > (WIFI_BUFF_SIZE - 40)))
    {
        lastFlushMicros = micros();
        if (serialLength > 0)
        {
            Serial.write(serialGVRET.getBufferedBytes(), serialLength);
            // Serial.write('\n');
            serialGVRET.clearBufferedBytes();
        }
        if (wifiLength > 0)
        {
            wifiManager.sendBufferedData();
        }
    }

    serialCnt = 0;
    while ((Serial.available() > 0) && serialCnt < 128)
    {
        serialCnt++;
        in_byte = Serial.read();
        DEBUG("Received byte: %02X\n", in_byte);
        serialGVRET.processIncomingByte(in_byte);
    }

    elmEmulator.loop();
}
