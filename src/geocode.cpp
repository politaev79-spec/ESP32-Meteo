#include "state.h"
#include "geocode.h"
#include "log.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

static String urlEncode(const String &s) {
    const char *hex = "0123456789ABCDEF";
    String o;
    o.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
        else { o += '%'; o += hex[(c >> 4) & 0xF]; o += hex[c & 0xF]; }
    }
    return o;
}

static String httpsGet(const String &url) {
    WiFiClientSecure client;
    client.setInsecure();   // без проверки сертификата (личный проект)
    HTTPClient http;
    if (!http.begin(client, url)) {
        LOG.println("[geocode] begin failed");
        return "[]";
    }
    http.addHeader("User-Agent", "ESP32-Meteo/1.0");
    http.addHeader("Accept", "application/json");
    http.setTimeout(15000);
    int code = http.GET();
    LOG.printf("[geocode] code=%d url=%s\r\n", code, url.c_str());
    String body = http.getString();
    if (code != 200) {
        LOG.printf("[geocode] error body: %s\r\n", body.c_str());
        http.end();
        return "[]";
    }
    http.end();
    return body;
}

static void handleGeocode() {
    String q = server.arg("q");
    if (q.length() == 0) { server.send(200, "application/json", "[]"); return; }
    String url = "https://photon.komoot.io/api/?q=" + urlEncode(q) + "&limit=15";
    server.send(200, "application/json", httpsGet(url));
}

static void handleElevation() {
    String lat = server.arg("lat"), lon = server.arg("lon");
    if (lat.length() == 0 || lon.length() == 0) { server.send(200, "application/json", "{}"); return; }
    String url = "https://api.open-elevation.com/api/v1/lookup?locations=" + lat + "," + lon;
    server.send(200, "application/json", httpsGet(url));
}

void geocodeBegin() {
    server.on("/geocode", HTTP_GET, handleGeocode);
    server.on("/elevation", HTTP_GET, handleElevation);
}
