#include "state.h"
#include "wifi_mgr.h"
#include "log.h"
#include "utils.h"
#include <WiFi.h>
#include <ESPmDNS.h>

static int staIdx = 0;
static unsigned long staLastTry = 0;
static bool staWasConnected = false;

void wifiApply() {
    WiFi.disconnect(true);          // сброс STA
    WiFi.softAPdisconnect(true);    // сброс AP
    delay(120);

    // AP — всегда «бонус»: точка доступа работает постоянно, чтобы станция
    // была доступна по 192.168.x.1 в любой момент.
    if (g_netMode == 0) WiFi.mode(WIFI_AP);      // только своя сеть
    else                WiFi.mode(WIFI_AP_STA);  // AP + домашний интернет
    WiFi.setSleep(false);

    // Логируем подключение/отключение клиентов точки доступа (диагностика).
    static bool evReg = false;
    if (!evReg) {
        WiFi.onEvent([](WiFiEvent_t e, WiFiEventInfo_t) {
            if (e == ARDUINO_EVENT_WIFI_AP_STACONNECTED)      LOG.println("[AP] client connected");
            else if (e == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) LOG.println("[AP] client disconnected");
        });
        evReg = true;
    }

    WiFi.softAPConfig(IPAddress(AP_IP), IPAddress(AP_GATEWAY), IPAddress(AP_SUBNET));
    WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, false, AP_MAX_CONNECT);
    LOG.printf("AP: %s  IP: %s\r\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // mDNS: доступ по короткому имени http://esp.local (и в AP, и в домашней сети)
    if (MDNS.begin("esp")) {
        MDNS.addService("http", "tcp", WEB_PORT);
        LOG.println("mDNS: http://esp.local");
    } else {
        LOG.println("mDNS: failed");
    }

    // STA (интернет): начинаем первое подключение в фоне (без блокировки).
    staIdx = 0;
    staLastTry = millis();
    staWasConnected = false;
    if (g_netMode != 0 && g_netCnt > 0) {
        LOG.printf("STA: try \"%s\"...\r\n", g_ssid[0].c_str());
        WiFi.begin(g_ssid[0].c_str(), g_pwd[0].c_str());
        staIdx = 1;
    } else if (g_netMode != 0) {
        LOG.println("STA: no saved networks, skip");
    }
}

void wifiPoll() {
    if (g_netMode == 0 || g_netCnt == 0) return;

    if (WiFi.status() == WL_CONNECTED) {
        if (!staWasConnected) {
            LOG.printf("STA: connected to %s, IP: %s\r\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
            staWasConnected = true;
        }
        return;
    }

    staWasConnected = false;
    // Перебираем сохранённые сети: раз в 5 с пробуем следующую.
    if (millis() - staLastTry >= 5000) {
        staLastTry = millis();
        if (staIdx >= g_netCnt) staIdx = 0;
        LOG.printf("STA: try \"%s\"...\r\n", g_ssid[staIdx].c_str());
        WiFi.begin(g_ssid[staIdx].c_str(), g_pwd[staIdx].c_str());
        staIdx++;
    }
}

String wifiScanJson() {
    int n = WiFi.scanNetworks();
    String ssids[40]; int sig[40]; int cnt = 0;
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i); s.trim();
        if (s.length() == 0) continue;                  // пропускаем скрытые
        int r = WiFi.RSSI(i);
        bool found = false;
        for (int j = 0; j < cnt; j++) {
            if (ssids[j] == s) { if (r > sig[j]) sig[j] = r; found = true; break; }
        }
        if (!found && cnt < 40) { ssids[cnt] = s; sig[cnt] = r; cnt++; }
    }
    for (int a = 0; a < cnt; a++)
        for (int b = a + 1; b < cnt; b++)
            if (sig[b] > sig[a]) { String ts = ssids[a]; ssids[a] = ssids[b]; ssids[b] = ts; int ti = sig[a]; sig[a] = sig[b]; sig[b] = ti; }

    String s = "[";
    for (int i = 0; i < cnt; i++) {
        if (i) s += ",";
        s += "{\"ssid\":\"" + jsonEscape(ssids[i]) + "\",\"rssi\":" + String(sig[i]) + "}";
    }
    s += "]";
    WiFi.scanDelete();
    return s;
}
