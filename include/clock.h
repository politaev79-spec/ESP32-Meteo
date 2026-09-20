#pragma once
#include <Arduino.h>

// Ручная установка даты/времени (у станции нет интернета => NTP недоступен).
// Время задаётся со страницы и сохраняется в NVS, чтобы пережить перезагрузку.
void     clockBegin();                               // восстановить время и пояс из NVS
void     clockSet(uint32_t epoch, int tzOffsetSec);  // задать UTC-время и пояс (и сохранить)
uint32_t clockEpoch();                               // текущее UTC-время (unix секунды)
int      clockTz();                                  // текущий часовой пояс (секунды от UTC)
