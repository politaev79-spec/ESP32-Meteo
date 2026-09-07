#ifndef DS18B20_H
#define DS18B20_H

#include <Arduino.h>
#include <OneWire.h>

// ===========================================================================
//  DS18B20 (KY-001) — чтение по 1-Wire через SKIP ROM с проверкой CRC.
//  Отбрасываем некорректные данные (CRC не сошёлся или temp вне диапазона),
//  чтобы «без датчика» показывалось "--", а не ложные 0.00.
//  Пин данных подтянут к 3.3V резистором 4.7к (KY-001 — на плате).
// ===========================================================================

class DS18B20 {
public:
    void begin(int pin) {
        _ds = new OneWire(pin);
    }

    // Возвращает true и заполняет temp (°C), если данные корректны (CRC ок).
    bool read(float &temp) {
        for (int attempt = 0; attempt < 3; attempt++) {   // до 3 попыток
            uint8_t data[9];
            if (!_ds->reset()) continue;                  // нет presence
            _ds->skip();                                  // SKIP ROM (0xCC)
            _ds->write(0x44, 1);                          // Convert T (power=1)
            delay(1000);
            if (!_ds->reset()) continue;
            _ds->skip();
            _ds->write(0xBE);                             // Read scratchpad
            for (int i = 0; i < 9; i++) data[i] = _ds->read();

            // Проверка CRC по scratchpad (байт 8 — контрольная сумма)
            if (crc8(data, 8) != data[8]) continue;

            int16_t raw = (int16_t)((data[1] << 8) | data[0]);
            // Диапазон: -70..+125 °C (расширили вниз под ожидаемый -65 °C)
            // => raw -1120..2000. Формально DS18B20 специфицирован до -55 °C,
            // но ниже он тоже читается (с чуть меньшей точностью).
            if (raw < -1120 || raw > 2000) continue;

            uint8_t cfg = data[4] & 0x60;
            if      (cfg == 0x00) raw &= ~7;               // 9-бит
            else if (cfg == 0x20) raw &= ~3;               // 10-бит
            else if (cfg == 0x40) raw &= ~1;               // 11-бит
            // 12-бит — как есть

            temp = (float)raw / 16.0;
            return true;
        }
        return false;
    }

private:
    OneWire *_ds = nullptr;

    // Dallas/Maxim 1-Wire CRC8 (полином x^8+x^5+x^4+1, отражённый 0x8C).
    static uint8_t crc8(const uint8_t *data, int len) {
        uint8_t crc = 0;
        for (int i = 0; i < len; i++) {
            crc ^= data[i];
            for (int b = 0; b < 8; b++) {
                if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
                else            crc >>= 1;
            }
        }
        return crc;
    }
};

#endif // DS18B20_H
