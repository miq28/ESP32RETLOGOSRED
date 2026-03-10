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
// #include <esp32_can.h>
#include "can_driver.h"
#include <SPI.h>
#include <Preferences.h>
#include "ELM327_Emulator.h"
#include "SerialConsole.h"
#include "wifi_manager.h"
#include "gvret_comm.h"
#include "can_manager.h"
#include "lawicel.h"
#include "esp_task_wdt.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "led_manager.h"

SemaphoreHandle_t serialMutex;

// Define the WDT timeout in seconds
#define WDT_TIMEOUT 10

// Converts reason type to a C string.
// Type is located in /tools/sdk/esp32/include/esp_system/include/esp_system.h
const char *resetReasonName(esp_reset_reason_t r)
{
    switch (r)
    {
    case ESP_RST_UNKNOWN:
        return "Unknown";
    case ESP_RST_POWERON:
        return "PowerOn"; // Power on or RST pin toggled
    case ESP_RST_EXT:
        return "ExtPin"; // External pin - not applicable for ESP32
    case ESP_RST_SW:
        return "Reboot"; // esp_restart()
    case ESP_RST_PANIC:
        return "Crash"; // Exception/panic
    case ESP_RST_INT_WDT:
        return "WDT_Int"; // Interrupt watchdog (software or hardware)
    case ESP_RST_TASK_WDT:
        return "WDT_Task"; // Task watchdog
    case ESP_RST_WDT:
        return "WDT_Other"; // Other watchdog
    case ESP_RST_DEEPSLEEP:
        return "Sleep"; // Reset after exiting deep sleep mode
    case ESP_RST_BROWNOUT:
        return "BrownOut"; // Brownout reset (software or hardware)
    case ESP_RST_SDIO:
        return "SDIO"; // Reset over SDIO
    default:
        return "";
    }
}

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

// CAN_COMMON *canBuses[NUM_BUSES];

// initializes all the system EEPROM values. Chances are this should be broken out a bit but
// there is only one checksum check for all of them so it's simple to do it all here.
void loadSettings()
{
    Logger::console("Loading settings....");

    nvPrefs.begin(PREF_NAME, false);

    settings.useBinarySerialComm = nvPrefs.getBool("binarycomm", false);
    settings.logLevel = nvPrefs.getUChar("loglevel", 1); // info
    settings.wifiMode = nvPrefs.getUChar("wifiMode", 1); // Wifi defaults to creating an AP
    settings.enableBT = nvPrefs.getBool("enable-bt", false);
    settings.enableLawicel = nvPrefs.getBool("enableLawicel", true);
    settings.systemType = 0;

    if (settings.systemType == 0)
    {
        // canBuses[0] = &CAN0;
        SysSettings.logToggle = false;
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
        // CAN0.setCANPins(GPIO_NUM_26, GPIO_NUM_27);
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

    SAVVYPORT.begin(1000000);
    DEBUGPORT.begin(115200);

    // Create the mutex before using it
    serialMutex = xSemaphoreCreateMutex();

    esp_reset_reason_t r = esp_reset_reason();
    SAVVYPORT.printf("\r\nReset reason %i - %s\r\n\r\n", r, resetReasonName(r));
    DEBUG("\r\nReset reason %i - %s\r\n\r\n", r, resetReasonName(r));
    DEBUG("Free heap before setup: %u\n", ESP.getFreeHeap());

    ledInit(20); // 0-255 set brightness

    // // 1. Ensure any previous watchdog config is removed
    // esp_task_wdt_deinit();

    // // 2. Define the configuration structure
    // esp_task_wdt_config_t wdt_config = {
    //     .timeout_ms = WDT_TIMEOUT * 1000, // Convert seconds to milliseconds
    //     .idle_core_mask = (1 << 0) | (1 << 1),// Monitor idle tasks on both cores
    //     .trigger_panic = true // Trigger a panic if the WDT timeout occurs
    // };

    // // 3. Initialize the WDT with the configuration structure
    // ESP_ERROR_CHECK(esp_task_wdt_init(&wdt_config));

    // // 4. Add current task (loopTask) to be watched
    // esp_task_wdt_add(NULL);

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

    // allow system to stabilize before creating tasks
    delay(200);

    // Create CAN RX task (Core 1)
    xTaskCreatePinnedToCore(
        canRxTask,
        "canRxTask",
        4096,
        NULL,
        3,
        NULL,
        1);

    // Create transport task (Core 0)
    xTaskCreatePinnedToCore(
        transportTask,
        "transportTask",
        6144,
        NULL,
        2,
        NULL,
        0);

    SysSettings.lawicelMode = false;
    SysSettings.lawicelAutoPoll = false;
    SysSettings.lawicelTimestamping = false;
    SysSettings.lawicelPollCounter = 0;

    DEBUG("Free heap after setup: %u\n", ESP.getFreeHeap());
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

    // canManager.loop();
    wifiManager.loop();

    size_t wifiLength = wifiGVRET.numAvailableBytes();
    size_t serialLength = serialGVRET.numAvailableBytes();
    size_t maxLength = (wifiLength > serialLength) ? wifiLength : serialLength;

    if ((micros() - lastFlushMicros > SER_BUFF_FLUSH_INTERVAL) || (maxLength > (WIFI_BUFF_SIZE - 40)))
    {
        // Serial.printf("wifiLength: %d and serialLength: %d\n", wifiLength, serialLength);
        lastFlushMicros = micros();
        if (serialLength > 0)
        {
            Serial.write(serialGVRET.getBufferedBytes(), serialLength);
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
        serialGVRET.processIncomingByte(in_byte);
    }

    elmEmulator.loop();

    // // 5. Reset the watchdog periodically
    // esp_task_wdt_reset();
    // delay(1); // Mandatory for WDT reset to apply in some versions
}
