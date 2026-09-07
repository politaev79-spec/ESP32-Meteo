#include "history.h"
#include "utils.h"
#include <time.h>
#include <LittleFS.h>
#include <Preferences.h>

// Кольцевая запись во Flash (LittleFS). Файл фиксированного размера, при
// заполнении старые записи затираются по кругу (перезапись самых старых).

struct Sample {
    uint32_t ts;      // unix-секунды
    int16_t  outT;    // 0.1 °C
    int16_t  inT;     // 0.1 °C
    int16_t  press;   // 0.1 мм рт.ст.
    int16_t  alt;     // 0.1 м
};

static const int CAPACITY = 35040;      // ~365 дней при 15-мин интервале (полный год)
static const char *HIST_FILE = "/history.bin";

static Preferences hPrefs;
static uint32_t g_head = 0;    // позиция следующей записи (0..CAPACITY-1)
static uint32_t g_count = 0;   // сколько накоплено (до заполнения кольца)

static uint32_t nowTs() {
    time_t t = time(nullptr);
    return (t > 1577836800) ? (uint32_t)t : (uint32_t)(millis() / 1000ULL);
}

void historyBegin() {
    if (!LittleFS.begin(true)) {
        return;   // без файловой системы история не пишется
    }
    hPrefs.begin("hist", false);
    g_head  = hPrefs.getUInt("head", 0);
    g_count = hPrefs.getUInt("count", 0);
    if (g_count > CAPACITY) g_count = CAPACITY;
    if (g_head >= CAPACITY) g_head = 0;

    // Предвыделяем файл до CAPACITY * sizeof(Sample) (один раз).
    size_t need = (size_t)CAPACITY * sizeof(Sample);
    File f = LittleFS.open(HIST_FILE, "r");
    if (!f || f.size() < need) {
        if (f) f.close();
        f = LittleFS.open(HIST_FILE, "r+");
        if (!f) f = LittleFS.open(HIST_FILE, "w");
        if (f) { f.seek(need - 1); f.write((uint8_t)0); f.close(); }
    } else {
        f.close();
    }
}

void historyAdd(float outT, float inT, float pressPa, float altM) {
    Sample s;
    s.ts = nowTs();
    s.outT  = (int16_t)lroundf(outT * 10.0f);
    s.inT   = (int16_t)lroundf(inT * 10.0f);
    s.press = (int16_t)lroundf((pressPa / 133.322f) * 10.0f);
    s.alt   = (int16_t)lroundf(altM * 10.0f);

    File f = LittleFS.open(HIST_FILE, "r+");
    if (f) {
        f.seek((size_t)g_head * sizeof(Sample));
        f.write((const uint8_t *)&s, sizeof(Sample));
        f.close();
    }

    g_head = (g_head + 1) % CAPACITY;
    if (g_count < CAPACITY) g_count++;
    hPrefs.putUInt("head", g_head);
    hPrefs.putUInt("count", g_count);
}

// файловая позиция сэмпла по хронологическому индексу (0 = самый старый)
static uint32_t chronPos(uint32_t i) {
    if (g_count < CAPACITY) return i;
    return (g_head + i) % CAPACITY;
}

// читает один сэмпл по хронологическому индексу из уже открытого файла
static bool readChron(File &f, uint32_t i, Sample &out) {
    f.seek((size_t)chronPos(i) * sizeof(Sample));
    return f.read((uint8_t *)&out, sizeof(Sample)) == sizeof(Sample);
}

static void fmtLabel(uint32_t ts, bool timeOnly, String &out) {
    char b[16];
    if (ts > 1577836800) {
        struct tm tmv;
        localtime_r((const time_t *)&ts, &tmv);
        if (timeOnly) snprintf(b, sizeof(b), "%02d:%02d", tmv.tm_hour, tmv.tm_min);
        else          snprintf(b, sizeof(b), "%02d.%02d", tmv.tm_mday, tmv.tm_mon + 1);
    } else {
        uint32_t s = ts % 86400;
        if (timeOnly) snprintf(b, sizeof(b), "%02lu:%02lu", (unsigned long)(s / 3600), (unsigned long)((s % 3600) / 60));
        else          snprintf(b, sizeof(b), "t+%lud", (unsigned long)(ts / 86400));
    }
    out = b;
}

static float sampleVal(const Sample &s, const String &m) {
    if (m == "out") return s.outT / 10.0f;
    if (m == "in")  return s.inT / 10.0f;
    if (m == "press") return s.press / 10.0f;
    return s.alt / 10.0f;
}

static float aggVal(int64_t so, int64_t si, int64_t sp, int64_t sa, uint16_t nn, const String &m) {
    if (m == "out") return so / (float)nn / 10.0f;
    if (m == "in")  return si / (float)nn / 10.0f;
    if (m == "press") return sp / (float)nn / 10.0f;
    return sa / (float)nn / 10.0f;
}

String historyJson(const String &metric, const String &range) {
    File f = LittleFS.open(HIST_FILE, "r");
    if (!f) return "{\"labels\":[],\"data\":[]}";

    String labels = "[", data = "[";
    bool first = true;

    if (range == "year") {
        // Агрегируем по дням (средние) — проходим весь файл последовательно.
        labels.reserve(400 * 8);
        data.reserve(400 * 7);
        uint32_t curDay = 0;
        int64_t so = 0, si = 0, sp = 0, sa = 0;
        uint16_t nn = 0;
        for (uint32_t i = 0; i < g_count; i++) {
            Sample s;
            if (!readChron(f, i, s)) continue;
            uint32_t dk = s.ts / 86400;
            if (curDay == 0) curDay = dk;
            if (dk != curDay) {
                if (nn > 0) {
                    if (!first) { labels += ","; data += ","; }
                    first = false;
                    String lb; fmtLabel(curDay * 86400, false, lb);
                    labels += "\"" + lb + "\"";
                    data += String(aggVal(so, si, sp, sa, nn, metric), 1);
                }
                curDay = dk; so = si = sp = sa = 0; nn = 0;
            }
            so += s.outT; si += s.inT; sp += s.press; sa += s.alt; nn++;
        }
        if (nn > 0) {
            if (!first) { labels += ","; data += ","; }
            String lb; fmtLabel(curDay * 86400, false, lb);
            labels += "\"" + lb + "\"";
            data += String(aggVal(so, si, sp, sa, nn, metric), 1);
        }
    } else {
        // day / week / month — последние N сэмплов
        uint32_t want = 96;      // день
        bool timeOnly = true;
        if (range == "week")  { want = 672;  timeOnly = false; }
        else if (range == "month") { want = 2880; timeOnly = false; }

        uint32_t start = (g_count > want) ? (g_count - want) : 0;
        uint32_t n = g_count - start;
        labels.reserve(n * 8);
        data.reserve(n * 7);
        for (uint32_t i = start; i < g_count; i++) {
            Sample s;
            if (!readChron(f, i, s)) continue;
            if (!first) { labels += ","; data += ","; }
            first = false;
            String lb; fmtLabel(s.ts, timeOnly, lb);
            labels += "\"" + lb + "\"";
            data += String(sampleVal(s, metric), 1);
        }
    }

    f.close();
    labels += "]"; data += "]";
    return "{\"labels\":" + labels + ",\"data\":" + data + "}";
}
