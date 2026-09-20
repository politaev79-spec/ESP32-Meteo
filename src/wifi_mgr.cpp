#include "state.h"
#include "wifi_mgr.h"
#include "log.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

static DNSServer dns;               // Captive Portal: отвечает на любой домен нашим IP

void wifiApply() {
    // Автономный режим: только точка доступа. Без STA нет переключений канала
    // и фоновых сканирований — AP поднимается быстро и держится стабильно.
    WiFi.persistent(false);                     // не писать в NVS на каждое изменение
    WiFi.disconnect(true);
    WiFi.softAPdisconnect(true);
    delay(80);

    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);                       // без энергосбережения: ниже задержки
    WiFi.setTxPower(WIFI_POWER_19_5dBm);        // максимальная мощность передатчика

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
    WiFi.softAP(g_apSsid.c_str(), nullptr, AP_CHANNEL, false, AP_MAX_CONNECT);
    LOG.printf("AP: %s  IP: %s\r\n", g_apSsid.c_str(), WiFi.softAPIP().toString().c_str());

    // mDNS: короткое имя http://esp.local
    if (MDNS.begin("esp")) {
        MDNS.addService("http", "tcp", WEB_PORT);
        LOG.println("mDNS: http://esp.local");
    } else {
        LOG.println("mDNS: failed");
    }

    // ---- Captive Portal: DNS отвечает на ЛЮБОЙ домен нашим IP ----
    // Тогда телефон при подключении сам покажет «Войти в сеть» и откроет нашу страницу.
    dns.stop();
    if (dns.start(53, "*", WiFi.softAPIP())) {
        LOG.printf("Captive Portal: DNS * -> %s\r\n", WiFi.softAPIP().toString().c_str());
    } else {
        LOG.println("Captive Portal: DNS не запустился");
    }
}

void wifiDnsPoll() {
    dns.processNextRequest();
}

