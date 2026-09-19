#include "state.h"
#include "settings.h"
#include <Preferences.h>

static Preferences prefs;

// Определения настроек (extern в state.h)
float g_lat    = LATITUDE;
float g_lon    = LONGITUDE;
float g_refAlt = REF_ALTITUDE;
float g_dsOff  = DS18B20_CAL_OFFSET;
float g_bmpOff = BMP_TEMP_CAL_OFFSET;

void settingsLoad() {
    prefs.begin("meteo", false);
    g_lat    = prefs.isKey("lat")    ? prefs.getFloat("lat", LATITUDE)              : LATITUDE;
    g_lon    = prefs.isKey("lon")    ? prefs.getFloat("lon", LONGITUDE)             : LONGITUDE;
    g_refAlt = prefs.isKey("refalt") ? prefs.getFloat("refalt", REF_ALTITUDE)       : REF_ALTITUDE;
    g_dsOff  = prefs.isKey("dsoff")  ? prefs.getFloat("dsoff", DS18B20_CAL_OFFSET)  : DS18B20_CAL_OFFSET;
    g_bmpOff = prefs.isKey("bmpoff") ? prefs.getFloat("bmpoff", BMP_TEMP_CAL_OFFSET) : BMP_TEMP_CAL_OFFSET;
}

void settingsSave() {
    prefs.putFloat("lat", g_lat);
    prefs.putFloat("lon", g_lon);
    prefs.putFloat("refalt", g_refAlt);
    prefs.putFloat("dsoff", g_dsOff);
    prefs.putFloat("bmpoff", g_bmpOff);
}
