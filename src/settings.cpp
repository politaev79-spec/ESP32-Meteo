#include "state.h"
#include "settings.h"
#include <Preferences.h>

static Preferences prefs;

// Определения настроек (extern в state.h)
float g_refAlt = REF_ALTITUDE;
float g_dsOff  = DS18B20_CAL_OFFSET;
float g_bmpOff = BMP_TEMP_CAL_OFFSET;
String g_apSsid = AP_SSID;              // имя точки доступа (по умолчанию из config.h)

void settingsLoad() {
    prefs.begin("meteo", false);
    g_refAlt = prefs.isKey("refalt") ? prefs.getFloat("refalt", REF_ALTITUDE)       : REF_ALTITUDE;
    g_dsOff  = prefs.isKey("dsoff")  ? prefs.getFloat("dsoff", DS18B20_CAL_OFFSET)  : DS18B20_CAL_OFFSET;
    g_bmpOff = prefs.isKey("bmpoff") ? prefs.getFloat("bmpoff", BMP_TEMP_CAL_OFFSET) : BMP_TEMP_CAL_OFFSET;
    g_apSsid = prefs.isKey("apssid") ? prefs.getString("apssid", AP_SSID) : String(AP_SSID);
    g_apSsid.trim();
    if (g_apSsid.length() == 0 || g_apSsid.length() > 32) g_apSsid = AP_SSID;   // защита от мусора в NVS
}

void settingsSave() {
    prefs.putFloat("refalt", g_refAlt);
    prefs.putFloat("dsoff", g_dsOff);
    prefs.putFloat("bmpoff", g_bmpOff);
    prefs.putString("apssid", g_apSsid);
}
