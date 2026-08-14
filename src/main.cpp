#include <Arduino.h>
#include <HWCDC.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config.h"
#include "bmp280.h"
#include "aht20.h"

HWCDC usbLog;
#define LOG usbLog

BMP280 bmp;
AHT20  aht;

WebServer server(WEB_PORT);

volatile float g_temp = 0.0f;
volatile float g_hum  = 0.0f;
volatile float g_press = 0.0f;   // Pa
volatile float g_alt = 0.0f;     // m
volatile bool  g_ok = false;
volatile uint32_t g_lastRead = 0;

// Высота по давлению (барометрическая формула).
static float calcAltitude(float pressurePa) {
    if (pressurePa <= 0) return 0;
    return 44330.0f * (1.0f - powf(pressurePa / 101325.0f, 0.190295f));
}

static void handleRoot() {
    static const char page[] PROGMEM = R"rawhtml(<!DOCTYPE html>
<html lang="ru"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-C3 Метеостанция</title>
<style>
:root{--bg:#0f1115;--panel:#171a21;--line:#2a2f3a;--txt:#e6e9ef;--dim:#8a93a6;--acc:#3d9bff}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--txt);font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;min-height:100vh}
.wrap{max-width:760px;margin:0 auto;padding:20px}
h1{font-size:1.2rem;font-weight:600;margin-bottom:16px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:20px}
.card .lbl{color:var(--dim);font-size:.8rem;margin-bottom:8px}
.card .val{font-size:2rem;font-weight:700}
.card .unit{color:var(--dim);font-size:1rem}
.card .sub{color:var(--dim);font-size:.8rem;margin-top:6px}
.off{color:#e5484d;background:rgba(229,72,77,.12);border:1px solid #e5484d;padding:12px;border-radius:10px;font-weight:600;margin-bottom:14px}
.ref{color:var(--dim);font-size:.8rem;margin-top:16px}
@media(max-width:520px){.card .val{font-size:1.6rem}}
</style></head>
<body><div class="wrap">
<h1>🌡 Метеостанция (ESP32-C3 + AHT20/BMP280)</h1>
<div id="off" class="off" style="display:none">Датчики не найдены. Проверь I2C-пины и питание.</div>
<div class="grid">
  <div class="card"><div class="lbl">Температура</div><div class="val"><span id="t">--</span> <span class="unit">°C</span></div></div>
  <div class="card"><div class="lbl">Влажность</div><div class="val"><span id="h">--</span> <span class="unit">%</span></div></div>
  <div class="card"><div class="lbl">Давление</div><div class="val"><span id="p">--</span> <span class="unit">гПа</span></div><div class="sub">—</div></div>
  <div class="card"><div class="lbl">Высота</div><div class="val"><span id="a">--</span> <span class="unit">м</span></div></div>
</div>
<div class="ref">Обновление: раз в 2 с · AP: ESP32-Meteo · 192.168.4.1</div>
</div>
<script>
function load(){fetch('/api').then(r=>r.json()).then(d=>{
  var off=document.getElementById('off');
  if(d.ok){off.style.display='none';
    document.getElementById('t').textContent=d.temp.toFixed(1);
    document.getElementById('h').textContent=d.hum.toFixed(1);
    document.getElementById('p').textContent=d.press.toFixed(1);
    document.getElementById('a').textContent=d.alt.toFixed(1);
  } else { off.style.display='block'; }
}).catch(()=>{document.getElementById('off').style.display='block';});}
load(); setInterval(load,2000);
</script>
</body></html>)rawhtml";
    server.send_P(200, "text/html; charset=utf-8", page);
}

static void handleApi() {
    String s = "{\"ok\":";
    s += g_ok ? "true" : "false";
    s += ",\"temp\":" + String(g_temp, 2);
    s += ",\"hum\":" + String(g_hum, 1);
    s += ",\"press\":" + String(g_press / 100.0f, 1);
    s += ",\"alt\":" + String(g_alt, 1);
    s += "}";
    server.send(200, "application/json", s);
}

void setup() {
    LOG.begin(115200);
    delay(200);
    LOG.println("ESP32-C3 Метеостанция (AHT20 + BMP280)");

    // ---- WiFi ----
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.softAP(AP_SSID, nullptr, AP_CHANNEL, false, AP_MAX_CONNECT);
    WiFi.softAPConfig(IPAddress(AP_IP), IPAddress(AP_GATEWAY), IPAddress(AP_SUBNET));
    LOG.printf("AP: %s  IP: %s\r\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    if (strlen(STA_SSID) > 0) {
        WiFi.begin(STA_SSID, STA_PASSWORD);
        LOG.printf("STA: connecting to %s...\r\n", STA_SSID);
    }

    // ---- I2C init (one clean setup on GPIO8/9) ----
    Wire.begin(BMP_SDA_PIN, BMP_SCL_PIN, BMP_I2C_FREQ);
    gpio_set_pull_mode((gpio_num_t)BMP_SDA_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)BMP_SCL_PIN, GPIO_PULLUP_ONLY);

    // ---- I2C scan (single pass) ----
    LOG.printf("--- I2C scan on SDA=%d SCL=%d ---\r\n", BMP_SDA_PIN, BMP_SCL_PIN);
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0)
            LOG.printf("  device at 0x%02X\r\n", a);
    }
    LOG.println("--- end scan ---");

    // ---- AHT20 (temp/humidity) ----
    if (aht.begin(AHT20_ADDR)) {
        LOG.printf("AHT20 OK at 0x%02X\r\n", AHT20_ADDR);
        g_ok = true;
    } else {
        LOG.printf("ERROR: AHT20 not found at 0x%02X\r\n", AHT20_ADDR);
    }

    // ---- BMP280 (pressure), try 0x76 then 0x77 ----
    uint8_t addr = 0x76;
    if (!bmp.begin(BMP_SDA_PIN, BMP_SCL_PIN, 0x76, BMP_I2C_FREQ)) {
        addr = 0x77;
        if (!bmp.begin(BMP_SDA_PIN, BMP_SCL_PIN, 0x77, BMP_I2C_FREQ)) {
            LOG.println("ERROR: BMP280 not found (tried 0x76, 0x77)");
        } else {
            LOG.println("BMP280 OK at 0x77");
            g_ok = true;
        }
    } else {
        LOG.printf("BMP280 OK at 0x%02X\r\n", addr);
        g_ok = true;
    }

    // ---- Web ----
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api", HTTP_GET, handleApi);
    server.begin();
    LOG.println("Web server: http://" + WiFi.softAPIP().toString());
}

void loop() {
    server.handleClient();

    if ((uint32_t)(millis() - g_lastRead) >= SENSOR_INTERVAL_MS) {
        g_lastRead = millis();

        float t_aht, h, t_bmp, p;
        bool okAht = aht.read(t_aht, h);
        bool okBmp = bmp.read(t_bmp, p);

        if (okAht || okBmp) {
            g_ok = true;
            if (okAht) { g_temp = t_aht; g_hum = h; }
            if (okBmp) { g_press = p; g_alt = calcAltitude(p); }
            LOG.printf("[OK] T=%.2f C H=%.1f%% P=%.2f hPa Alt=%.1f m\r\n",
                       g_temp, g_hum, g_press / 100.0f, g_alt);
        } else {
            g_ok = false;
            LOG.println("[ERR] sensors read failed");
        }
    }
    delay(1);
}
