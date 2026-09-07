#include "state.h"
#include "static_files.h"
#include <LittleFS.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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

// Прокси тайла OpenStreetMap: /tile?z=..&x=..&y=..  (интернет станции)
static void handleTile() {
    String z = server.arg("z"), x = server.arg("x"), y = server.arg("y");
    if (z.length() == 0 || x.length() == 0 || y.length() == 0) { server.send(400, "text/plain", "bad"); return; }
    String osm = "https://tile.openstreetmap.org/" + z + "/" + x + "/" + y + ".png";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    if (!http.begin(client, osm)) { server.send(502, "text/plain", "b"); return; }
    http.addHeader("User-Agent", "ESP32-Meteo/1.0");
    http.setTimeout(10000);
    int code = http.GET();
    if (code != 200) { http.end(); server.send(502, "text/plain", "e"); return; }
    int len = http.getSize();
    if (len <= 0 || len > 120000) { http.end(); server.send(502, "text/plain", "len"); return; }

    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) { http.end(); server.send(502, "text/plain", "mem"); return; }
    WiFiClient *stream = http.getStreamPtr();
    size_t got = 0;
    while (got < (size_t)len && stream && stream->available()) {
        got += stream->readBytes(buf + got, len - got);
    }
    http.end();
    server.send(200, "image/png", String((const char *)buf, got));
    free(buf);
}

void staticFilesBegin() {
    server.on("/leaflet.js", HTTP_GET, handleLeafletJs);
    server.on("/leaflet.css", HTTP_GET, handleLeafletCss);
    server.on("/chart.js", HTTP_GET, handleChartJs);
    server.on("/tile", HTTP_GET, handleTile);
}
