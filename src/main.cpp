#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "log.h"
#include "state.h"
#include "settings.h"
#include "sensors.h"
#include "wifi_mgr.h"
#include "ota.h"
#include "history.h"
#include "static_files.h"
#include <time.h>

HWCDC usbLog;

WebServer server(WEB_PORT);

volatile float g_temp = 0.0f;       // ДОМ (BMP280)
volatile float g_outTemp = -999.0f; // УЛИЦА (DS18B20); -999 = нет данных
volatile bool  g_hasDs = false;
volatile float g_press = 0.0f;      // Pa
volatile float g_alt = 0.0f;        // м
volatile bool  g_ok = false;
volatile float g_refPressure = 101325.0f;   // давление, приведённое к уровню моря (Па)
volatile uint32_t g_lastRead = 0;


static void handleApi() {
    String s = "{\"ok\":";
    s += g_ok ? "true" : "false";
    s += ",\"out\":" + String(g_hasDs ? g_outTemp : -999.0f, 2);
    s += ",\"temp\":" + String(g_temp, 2);
    s += ",\"press\":" + String(g_refPressure / 133.322f, 1);   // приведено к уровню моря (мм рт. ст.)
    s += ",\"pressAbs\":" + String(g_press / 133.322f, 1);      // абсолютное на станции (мм рт. ст.)
    s += ",\"alt\":" + String(g_alt, 1);
    s += ",\"lat\":" + String(g_lat, 6);
    s += ",\"lon\":" + String(g_lon, 6);
    s += ",\"refalt\":" + String(g_refAlt, 1);
    s += ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += ",\"apip\":\"" + WiFi.softAPIP().toString() + "\"";
    s += "}";
    server.send(200, "application/json", s);
}

// ---- Изменение настроек через Web UI ----
static void handleSave() {
    if (server.hasArg("lat"))    g_lat    = server.arg("lat").toFloat();
    if (server.hasArg("lon"))    g_lon    = server.arg("lon").toFloat();
    if (server.hasArg("refalt")) g_refAlt = server.arg("refalt").toFloat();
    if (server.hasArg("dsoff"))  g_dsOff  = server.arg("dsoff").toFloat();
    if (server.hasArg("bmpoff")) g_bmpOff = server.arg("bmpoff").toFloat();
    settingsSave();

    g_refPressure = calcSeaLevelPressure(g_press);   // пересчитать давление к уровню моря по новой высоте
    g_alt = g_refAlt;                                // реальная высота станции

    String s = "{";
    s += "\"lat\":" + String(g_lat, 6) + ",\"lon\":" + String(g_lon, 6);
    s += ",\"refalt\":" + String(g_refAlt, 1) + ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += "}";
    server.send(200, "application/json", s);
}

// ---- История показаний (графики) ----
static void handleHistory() {
    String m = server.hasArg("metric") ? server.arg("metric") : "out";
    String r = server.hasArg("range") ? server.arg("range") : "day";
    server.send(200, "application/json", historyJson(m, r));
}

void setup() {
    LOG.begin(115200);
    delay(200);
    LOG.println("ESP32-C3 Метеостанция (BMP280 + DS18B20)");

    settingsLoad();    // настройки из NVS (координаты, поправки)
    wifiApply();       // точка доступа (автономный режим)
    historyBegin();    // история показаний (кольцевой буфер)
    configTime(TZ_OFFSET_SECONDS, 0, "pool.ntp.org", "time.google.com");  // время по NTP
    sensorsBegin();    // датчики + первичное чтение

    // ---- Web ----
    server.on("/api", HTTP_GET, handleApi);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/history", HTTP_GET, handleHistory);
    staticFilesBegin(); // / и /dashboard — единая страница; /chart.js — графики
    otaBegin();     // /ota + /update (обновление прошивки)
    server.enableCORS(true);   // разрешаем обращаться к API с любого адреса
    server.begin();

    LOG.println("Web server: http://" + WiFi.softAPIP().toString());
}

void loop() {
    server.handleClient();
    uint32_t now = millis();

    // Живой опрос датчиков (частый) — вывод в терминал + свежие данные на странице
    if ((uint32_t)(now - g_lastRead) >= SENSOR_LIVE_MS) {
        g_lastRead = now;
        sensorsPoll();
    }

    // Запись в историю — раз в SENSOR_INTERVAL_MS (15 мин).
    // Первый семпл пишем сразу после старта.
    static uint32_t lastHist = (uint32_t)(0 - SENSOR_INTERVAL_MS);
    if ((uint32_t)(now - lastHist) >= SENSOR_INTERVAL_MS) {
        lastHist = now;
        historyAdd(g_outTemp, g_temp, g_refPressure, g_alt);   // давление к ур. моря + реальная высота
    }

    delay(1);
}
