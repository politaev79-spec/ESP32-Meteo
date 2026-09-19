#include "state.h"
#include "static_files.h"
#include <LittleFS.h>

static void handleLeafletJs() {
    File f = LittleFS.open("/leaflet.js", "r");
    if (!f) { server.send(404, "text/plain", "nf"); return; }
    server.streamFile(f, "application/javascript");
    f.close();
}

static void handleLeafletCss() {
    File f = LittleFS.open("/leaflet.css", "r");
    if (!f) { server.send(404, "text/plain", "nf"); return; }
    server.streamFile(f, "text/css");
    f.close();
}

static void handleChartJs() {
    File f = LittleFS.open("/chart.js", "r");
    if (!f) { server.send(404, "text/plain", "nf"); return; }
    server.streamFile(f, "application/javascript");
    f.close();
}

// Отдельная страница-дашборд (data/dashboard.html)
static void handleDashboard() {
    File f = LittleFS.open("/dashboard.html", "r");
    if (!f) { server.send(404, "text/html; charset=utf-8", "<h3>dashboard.html не загружен в LittleFS</h3>"); return; }
    server.streamFile(f, "text/html; charset=utf-8");
    f.close();
}

void staticFilesBegin() {
    server.on("/leaflet.js", HTTP_GET, handleLeafletJs);
    server.on("/leaflet.css", HTTP_GET, handleLeafletCss);
    server.on("/chart.js", HTTP_GET, handleChartJs);
    server.on("/dashboard", HTTP_GET, handleDashboard);
}
