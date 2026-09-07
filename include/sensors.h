#pragma once
#include <Arduino.h>

// Давление, приведённое к уровню моря по реальной высоте станции (g_refAlt).
float calcSeaLevelPressure(float p);
void sensorsBegin();     // I2C + датчики + первичное чтение
bool sensorsPoll();      // периодическое чтение; true если что-то прочитано
