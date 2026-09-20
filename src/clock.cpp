#include "clock.h"
#include "config.h"
#include "log.h"
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>

static Preferences prefs;
static int g_tz = TZ_OFFSET_SECONDS;

// POSIX TZ: локальное время = UTC + offset. Для UTC+3 строка "UTC-3" (знак инвертируется).
static void applyTz(int offsetSec) {
    int m  = offsetSec / 60;
    int ah = m / 60;      if (ah < 0) ah = -ah;
    int am = m % 60;      if (am < 0) am = -am;
    char buf[24];
    snprintf(buf, sizeof(buf), "UTC%c%d:%02d", offsetSec >= 0 ? '-' : '+', ah, am);
    setenv("TZ", buf, 1);
    tzset();
}

void clockSet(uint32_t epoch, int tzOffsetSec) {
    g_tz = tzOffsetSec;
    applyTz(g_tz);

    struct timeval tv;
    tv.tv_sec  = (time_t)epoch;
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);

    prefs.begin("clock", false);
    prefs.putUInt("epoch", epoch);
    prefs.putInt("tz", g_tz);

    LOG.printf("[clock] время задано: %lu, пояс UTC%+d c\r\n", (unsigned long)epoch, g_tz);
}

void clockBegin() {
    prefs.begin("clock", false);
    g_tz = prefs.getInt("tz", TZ_OFFSET_SECONDS);
    applyTz(g_tz);

    uint32_t e = prefs.getUInt("epoch", 0);
    if (e > 1000000000U) {              // сохранённое время есть (после 2001 г.)
        struct timeval tv;
        tv.tv_sec  = (time_t)e;
        tv.tv_usec = 0;
        settimeofday(&tv, nullptr);
        LOG.printf("[clock] время восстановлено из памяти: %lu\r\n", (unsigned long)e);
    } else {
        LOG.println("[clock] время не задано (графики будут без дат)");
    }
}

uint32_t clockEpoch() { return (uint32_t)time(nullptr); }
int      clockTz()    { return g_tz; }
