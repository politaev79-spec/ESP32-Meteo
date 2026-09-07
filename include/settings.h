#pragma once
#include <Arduino.h>

void settingsLoad();
void settingsSave();
int  networksFind(const String &ssid);
bool networksAdd(const String &ssid, const String &pwd);
void networksDel(int i);
String networksJson();
