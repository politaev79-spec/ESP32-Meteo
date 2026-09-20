#include "state.h"
#include "sensors.h"
#include "log.h"
#include <Wire.h>
#include "bmp280.h"
#include "ds18b20.h"

static BMP280 bmp;
static DS18B20 ds;

// Давление, приведённое к уровню моря по реальной высоте станции g_refAlt.
// Если высота не задана (<=0) или давление некорректно — возвращаем как есть (абсолютное).
float calcSeaLevelPressure(float p) {
    if (p <= 0) return p;
    if (g_refAlt <= 0) return p;                 // высота не задана — без коррекции
    float r = 1.0f - g_refAlt / 44330.0f;
    if (r <= 0) return p;
    return p / powf(r, 5.25588f);
}

void sensorsBegin() {
    // Настройка I2C
    Wire.begin(BMP_SDA_PIN, BMP_SCL_PIN, BMP_I2C_FREQ);
    gpio_set_pull_mode((gpio_num_t)BMP_SDA_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)BMP_SCL_PIN, GPIO_PULLUP_ONLY);

    // DS18B20
    ds.begin(DS18B20_PIN);
    LOG.printf("DS18B20 on GPIO%d\r\n", DS18B20_PIN);

    // I2C scan
    LOG.printf("--- I2C scan on SDA=%d SCL=%d ---\r\n", BMP_SDA_PIN, BMP_SCL_PIN);
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) LOG.printf("  device at 0x%02X\r\n", a);
    }
    LOG.println("--- end scan ---");

    // BMP280
    uint8_t addr = 0x76;
    if (!bmp.begin(BMP_SDA_PIN, BMP_SCL_PIN, 0x76, BMP_I2C_FREQ)) {
        addr = 0x77;
        if (!bmp.begin(BMP_SDA_PIN, BMP_SCL_PIN, 0x77, BMP_I2C_FREQ)) {
            LOG.println("ERROR: BMP280 not found (tried 0x76, 0x77)");
        } else { LOG.println("BMP280 OK at 0x77"); g_ok = true; }
    } else { LOG.printf("BMP280 OK at 0x%02X\r\n", addr); g_ok = true; }

    // Первичное чтение BMP280
    if (g_ok) {
        float t0, p0;
        if (bmp.read(t0, p0)) {
            g_temp = t0 + g_bmpOff;
            g_press = p0;
            g_refPressure = calcSeaLevelPressure(p0);   // к уровню моря
            g_alt = g_refAlt;                           // реальная высота станции
            g_lastRead = millis();
            LOG.printf("[OK] init BMP280 T=%.2f C P=%.2f (sea %.2f) mmHg Alt=%.1f m\r\n",
                       g_temp, p0 / 133.322f, g_refPressure / 133.322f, g_alt);
        }
    }

    // Первичное чтение DS18B20
    float tds;
    if (ds.read(tds)) {
        g_outTemp = tds + g_dsOff;
        g_hasDs = true;
        LOG.printf("[OK] init DS18B20 out=%.2f C\r\n", g_outTemp);
    } else {
        LOG.println("[WARN] DS18B20 not detected / bad CRC");
    }
}

static bool dsBusy = false;
static uint32_t dsStart = 0;

bool sensorsPoll() {
    // BMP280 — быстрый (I2C), читаем сразу
    float t_bmp, p;
    bool okBmp = bmp.read(t_bmp, p);

    // DS18B20 — неблокирующая конвертация (~750 мс): запускаем и читаем по готовности,
    // чтобы веб-сервер не «залипал» на delay(1000).
    bool okDs = false;
    if (dsBusy) {
        if ((uint32_t)(millis() - dsStart) >= 800) {
            float td;
            if (ds.readResult(td)) { g_outTemp = td + g_dsOff; g_hasDs = true; okDs = true; }
            dsBusy = false;
        }
    }
    if (!dsBusy) { dsBusy = ds.startConversion(); dsStart = millis(); }

    if (okBmp) {
        g_temp = t_bmp + g_bmpOff;
        g_press = p;
        g_refPressure = calcSeaLevelPressure(p);   // к уровню моря
        g_alt = g_refAlt;                           // реальная высота станции
    }
    g_ok = okBmp || g_hasDs;

    // ---- Живой вывод в терминал (отладка в реальном времени) ----
    uint32_t up = millis() / 1000;
    LOG.printf("[%02lu:%02lu:%02lu] Улица %6.2f C | Помещение %6.2f C | Давление: НА СТАНЦИИ %7.2f (абсолютное, мм рт.ст.) | приведено к ур. моря %7.2f | Высота %4.0f м\r\n",
               (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60), (unsigned long)(up % 60),
               g_hasDs ? g_outTemp : -999.0f, g_temp,
               g_press / 133.322f, g_refPressure / 133.322f, g_alt);
    return okBmp || okDs;
}
