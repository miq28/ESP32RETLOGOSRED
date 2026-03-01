#pragma once
#include <Arduino.h>

void ledInit(uint8_t brightness = 180);

void ledSetWifiConnected(bool connected);
void ledNotifyCanActivity();
void ledSetCanError(bool errorActive);

void ledTaskUpdate();   // call every 20–50ms