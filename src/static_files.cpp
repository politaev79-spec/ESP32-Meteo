#include "state.h"
#include "static_files.h"
#include <LittleFS.h>

static void handleChartJs() {
    server.sendHeader("Connection", "close");   // у ESP32-сервера один клиент за раз — не держим соединение
    File f = LittleFS.open("/chart.js", "r");
    if (!f) { server.send(404, "text/plain", "nf"); return; }
    server.streamFile(f, "application/javascript");
    f.close();
}

// Логотип в шапке страницы (data/logo.png). Файл статичный — разрешаем кэш на сутки.
static void handleLogo() {
    server.sendHeader("Connection", "close");   // у ESP32-сервера один клиент за раз — не держим соединение
    File f = LittleFS.open("/logo.png", "r");
    if (!f) { server.send(404, "text/plain", "nf"); return; }
    server.sendHeader("Cache-Control", "max-age=86400");
    server.streamFile(f, "image/png");
    f.close();
}

// Единая страница станции (data/index.html) — открывается по / и /dashboard
static void handleIndex() {
    server.sendHeader("Connection", "close");   // у ESP32-сервера один клиент за раз — не держим соединение
    File f = LittleFS.open("/index.html", "r");
    if (!f) { server.send(404, "text/html; charset=utf-8", "<h3>index.html не загружен в LittleFS</h3>"); return; }
    server.streamFile(f, "text/html; charset=utf-8");
    f.close();
}

void staticFilesBegin() {
    server.on("/", HTTP_GET, handleIndex);
    server.on("/dashboard", HTTP_GET, handleIndex);
    server.on("/chart.js", HTTP_GET, handleChartJs);
    server.on("/logo.png", HTTP_GET, handleLogo);
}
