#pragma once
#include <Arduino.h>

void historyBegin();
void historyAdd(float outT, float inT, float pressPa, float altM);
// metric: "out" | "in" | "press" | "alt";  range: "day" | "week" | "month" | "year"
String historyJson(const String &metric, const String &range);
