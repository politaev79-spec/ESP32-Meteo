#pragma once
#include <Arduino.h>

void wifiApply();          // поднять AP (сразу) + начать STA-подключение в фоне
void wifiPoll();           // обслуживание STA: реконнект/перебор сетей (вызывать в loop)
String wifiScanJson();     // вернуть список сетей JSON [{ssid,rssi}]
