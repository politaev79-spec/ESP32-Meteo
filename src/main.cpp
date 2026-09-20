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
#include "clock.h"
#include "utils.h"
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
    s += ",\"refalt\":" + String(g_refAlt, 1);
    s += ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += ",\"apip\":\"" + WiFi.softAPIP().toString() + "\"";
    s += ",\"apname\":\"" + jsonEscape(g_apSsid) + "\"";
    s += ",\"epoch\":" + String(clockEpoch()) + ",\"tz\":" + String(clockTz());
    s += "}";
    server.send(200, "application/json", s);
}

// ---- Изменение настроек через Web UI ----
static void handleSave() {
    if (server.hasArg("refalt")) g_refAlt = server.arg("refalt").toFloat();
    if (server.hasArg("dsoff"))  g_dsOff  = server.arg("dsoff").toFloat();
    if (server.hasArg("bmpoff")) g_bmpOff = server.arg("bmpoff").toFloat();
    settingsSave();

    g_refPressure = calcSeaLevelPressure(g_press);   // пересчитать давление к уровню моря по новой высоте
    g_alt = g_refAlt;                                // реальная высота станции

    String s = "{";
    s += "\"refalt\":" + String(g_refAlt, 1) + ",\"dsoff\":" + String(g_dsOff, 1);
    s += ",\"bmpoff\":" + String(g_bmpOff, 1);
    s += "}";
    server.send(200, "application/json", s);
}

// ---- Смена имени точки доступа ----
static bool     g_apRestartPending = false;
static uint32_t g_apRestartAt = 0;

static void handleSetAp() {
    if (!server.hasArg("name")) { server.send(400, "text/plain", "no name"); return; }
    String n = server.arg("name");
    n.trim();
    if (n.length() == 0 || n.length() > 32) { server.send(400, "text/plain", "bad name"); return; }

    g_apSsid = n;
    settingsSave();
    g_apRestartPending = true;                 // перезапустим AP чуть позже — чтобы ответ успел уйти
    g_apRestartAt = millis() + 1200;

    server.send(200, "application/json", "{\"ok\":true,\"apname\":\"" + jsonEscape(g_apSsid) + "\"}");
}

// ---- Установка даты/времени (для графиков) ----
static void handleSetTime() {
    if (!server.hasArg("epoch")) { server.send(400, "text/plain", "no epoch"); return; }
    uint32_t e = (uint32_t)strtoul(server.arg("epoch").c_str(), nullptr, 10);
    int tz = server.hasArg("tz") ? server.arg("tz").toInt() : clockTz();
    clockSet(e, tz);
    String s = "{\"ok\":true,\"epoch\":" + String(clockEpoch()) + ",\"tz\":" + String(clockTz()) + "}";
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
    clockBegin();      // дата/время из NVS (NTP недоступен — время задаётся со страницы)
    sensorsBegin();    // датчики + первичное чтение

    // ---- Web ----
    server.on("/api", HTTP_GET, handleApi);
    server.on("/save", HTTP_GET, handleSave);
    server.on("/history", HTTP_GET, handleHistory);
    server.on("/settime", HTTP_GET, handleSetTime);
    server.on("/setap", HTTP_GET, handleSetAp);
    staticFilesBegin(); // / и /dashboard — единая страница; /chart.js — графики
    otaBegin();     // /ota + /update (обновление прошивки)
    server.enableCORS(true);   // разрешаем обращаться к API с любого адреса
    server.begin();

    LOG.println("Web server: http://" + WiFi.softAPIP().toString());
}

void loop() {
    server.handleClient();
    uint32_t now = millis();

    // Перезапуск точки доступа после смены имени (клиент отключится — нужно переподключиться)
    if (g_apRestartPending && (int32_t)(now - g_apRestartAt) >= 0) {
        g_apRestartPending = false;
        LOG.println("[AP] перезапуск с новым именем...");
        wifiApply();
    }

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
        historyAdd(g_outTemp, g_temp, g_press, g_alt);   // в историю — давление НА СТАНЦИИ (абсолютное)
    }

    delay(1);
}
