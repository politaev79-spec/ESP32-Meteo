#pragma once
#include <Arduino.h>

void wifiApply();          // поднять точку доступа (AP) — автономный режим
void wifiDnsPoll();        // обслуживание DNS для Captive Portal (вызывать в loop)
