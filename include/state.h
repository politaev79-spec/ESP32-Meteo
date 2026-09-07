#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "config.h"

extern WebServer server;   // определяется в main.cpp, используется в модулях

// ---- Общее состояние, определяется по одному разу в соответствующих .cpp ----

// Считываемые показания (volatile — обновляются в sensors.cpp)
extern volatile float g_temp;           // ДОМ (BMP280)
extern volatile float g_outTemp;        // УЛИЦА (DS18B20)
extern volatile bool  g_hasDs;
extern volatile float g_press;          // Па
extern volatile float g_alt;            // м
extern volatile bool  g_ok;
extern volatile float g_refPressure;    // давление, приведённое к уровню моря (Па)
extern volatile uint32_t g_lastRead;

// Настройки (определяются в settings.cpp)
extern float g_lat, g_lon, g_refAlt, g_dsOff, g_bmpOff;
extern int   g_netMode;                 // 0=только AP, 2=AP+домашний Wi-Fi (интернет)
extern String g_ssid[MAX_NET];
extern String g_pwd[MAX_NET];
extern int   g_netCnt;
