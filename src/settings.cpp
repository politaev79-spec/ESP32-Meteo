#include "state.h"
#include "settings.h"
#include "utils.h"
#include <Preferences.h>

static Preferences prefs;

// Определения настроек (extern в state.h)
float g_lat    = LATITUDE;
float g_lon    = LONGITUDE;
float g_refAlt = REF_ALTITUDE;
float g_dsOff  = DS18B20_CAL_OFFSET;
float g_bmpOff = BMP_TEMP_CAL_OFFSET;
int   g_netMode = NET_MODE;
String g_ssid[MAX_NET];
String g_pwd[MAX_NET];
int   g_netCnt = 0;

void settingsLoad() {
    prefs.begin("meteo", false);
    g_lat    = prefs.isKey("lat")    ? prefs.getFloat("lat", LATITUDE)              : LATITUDE;
    g_lon    = prefs.isKey("lon")    ? prefs.getFloat("lon", LONGITUDE)             : LONGITUDE;
    g_refAlt = prefs.isKey("refalt") ? prefs.getFloat("refalt", REF_ALTITUDE)       : REF_ALTITUDE;
    g_dsOff  = prefs.isKey("dsoff")  ? prefs.getFloat("dsoff", DS18B20_CAL_OFFSET)  : DS18B20_CAL_OFFSET;
    g_bmpOff = prefs.isKey("bmpoff") ? prefs.getFloat("bmpoff", BMP_TEMP_CAL_OFFSET) : BMP_TEMP_CAL_OFFSET;
    g_netMode = prefs.isKey("netmode") ? prefs.getInt("netmode", NET_MODE) : NET_MODE;
    if (g_netMode == 1) g_netMode = 2;   // старый режим «только Wi-Fi» -> AP + Wi-Fi
    g_netCnt  = prefs.isKey("ncount")  ? prefs.getInt("ncount", 0) : 0;
    if (g_netCnt > MAX_NET) g_netCnt = MAX_NET;
    for (int i = 0; i < g_netCnt; i++) {
        String k = "nssid" + String(i), kp = "npwd" + String(i);
        g_ssid[i] = prefs.getString(k.c_str(), "");
        g_pwd[i]  = prefs.getString(kp.c_str(), "");
    }
}

void settingsSave() {
    prefs.putFloat("lat", g_lat);
    prefs.putFloat("lon", g_lon);
    prefs.putFloat("refalt", g_refAlt);
    prefs.putFloat("dsoff", g_dsOff);
    prefs.putFloat("bmpoff", g_bmpOff);
    prefs.putInt   ("netmode", g_netMode);
    prefs.putInt   ("ncount", g_netCnt);
    for (int i = 0; i < g_netCnt; i++) {
        String k = "nssid" + String(i), kp = "npwd" + String(i);
        prefs.putString(k.c_str(), g_ssid[i]);
        prefs.putString(kp.c_str(), g_pwd[i]);
    }
}

int networksFind(const String &ssid) {
    for (int i = 0; i < g_netCnt; i++) if (g_ssid[i] == ssid) return i;
    return -1;
}

bool networksAdd(const String &ssid, const String &pwd) {
    if (ssid.length() == 0 || pwd.length() == 0) return false;
    int idx = networksFind(ssid);
    if (idx < 0) {
        if (g_netCnt >= MAX_NET) return false;
        idx = g_netCnt;
        g_ssid[idx] = ssid; g_pwd[idx] = pwd; g_netCnt++;
    } else {
        g_pwd[idx] = pwd;
    }
    settingsSave();
    return true;
}

void networksDel(int i) {
    if (i < 0 || i >= g_netCnt) return;
    for (int j = i; j < g_netCnt - 1; j++) { g_ssid[j] = g_ssid[j + 1]; g_pwd[j] = g_pwd[j + 1]; }
    g_netCnt--;
    settingsSave();
}

String networksJson() {
    String s = "[";
    for (int i = 0; i < g_netCnt; i++) {
        if (i) s += ",";
        s += "{\"ssid\":\"" + jsonEscape(g_ssid[i]) + "\"}";
    }
    s += "]";
    return s;
}
