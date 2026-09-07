#ifndef BMP280_H
#define BMP280_H

#include <Arduino.h>
#include <Wire.h>

// ===========================================================================
//  BMP280 driver (I2C, native, no external library).
//  Reads temperature (°C) and pressure (Pa) using the datasheet integer math.
// ===========================================================================

class BMP280 {
public:
    bool begin(uint8_t sda, uint8_t scl, uint8_t addr = 0x76, uint32_t freq = 100000) {
        _addr = addr;
        Wire.begin(sda, scl, freq);
        Wire.setTimeOut(100);
        if (!read8(0xD0, _id)) return false;            // ID register
        if (_id != 0x58) return false;                  // BMP280 ID = 0x58
        if (!readCalibration()) return false;
        // ctrl_meas = osrs_t(1)<<5 | osrs_p(1)<<2 | mode(3=normal)
        write8(0xF4, 0x27);
        write8(0xF5, 0x00);                             // config: standby 0.5ms, filter off
        return true;
    }

    // Returns true and fills temp (C) and pressure (Pa) if read OK.
    bool read(float &temp, float &pressure) {
        int32_t adc_T, adc_P;
        if (!readRaw(0xFA, adc_T)) return false;      // температура
        temp = calcTemp(adc_T);

        // Усредняем несколько сэмплов давления, чтобы сгладить шум сенсора
        // (уменьшает «прыжки» вычисляемой барометрической высоты).
        int64_t psum = 0; int pcnt = 0;
        for (int i = 0; i < 5; i++) {
            if (readRaw(0xF7, adc_P)) { psum += calcPressure(adc_P); pcnt++; }
            delay(2);
        }
        if (pcnt == 0) return false;
        pressure = (float)psum / (float)pcnt;
        return true;
    }

private:
    uint8_t _addr = 0x76;
    uint8_t _id = 0;
    uint16_t dig_T1, dig_P1;
    int16_t  dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    int32_t  t_fine = 0;

    bool read8(uint8_t reg, uint8_t &out) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) return false;
        if (Wire.requestFrom((int)_addr, (int)1) != 1) return false;
        out = Wire.read();
        return true;
    }

    bool write8(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }

    bool readRaw(uint8_t reg, int32_t &out) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        if (Wire.endTransmission() != 0) return false;
        if (Wire.requestFrom((int)_addr, (int)3) != 3) return false;
        uint8_t b[3];
        b[0] = Wire.read(); b[1] = Wire.read(); b[2] = Wire.read();
        out = ((int32_t)b[0] << 12) | ((int32_t)b[1] << 4) | ((int32_t)b[2] >> 4);
        return true;
    }

    bool readCalibration() {
        // Temperature + pressure calibration at 0x88 (24 bytes)
        Wire.beginTransmission(_addr);
        Wire.write(0x88);
        if (Wire.endTransmission() != 0) return false;
        if (Wire.requestFrom((int)_addr, (int)24) != 24) return false;
        uint8_t c[24];
        for (int i = 0; i < 24; i++) c[i] = Wire.read();

        dig_T1 = (uint16_t)((c[1] << 8) | c[0]);
        dig_T2 = (int16_t)((c[3] << 8) | c[2]);
        dig_T3 = (int16_t)((c[5] << 8) | c[4]);
        dig_P1 = (uint16_t)((c[7] << 8) | c[6]);
        dig_P2 = (int16_t)((c[9] << 8) | c[8]);
        dig_P3 = (int16_t)((c[11] << 8) | c[10]);
        dig_P4 = (int16_t)((c[13] << 8) | c[12]);
        dig_P5 = (int16_t)((c[15] << 8) | c[14]);
        dig_P6 = (int16_t)((c[17] << 8) | c[16]);
        dig_P7 = (int16_t)((c[19] << 8) | c[18]);
        dig_P8 = (int16_t)((c[21] << 8) | c[20]);
        dig_P9 = (int16_t)((c[23] << 8) | c[22]);
        return true;
    }

    float calcTemp(int32_t adc_T) {
        int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1)) * ((int32_t)dig_T2)) >> 11);
        int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
        t_fine = var1 + var2;
        return (float)((t_fine * 5 + 128) >> 8) / 100.0f;
    }

    float calcPressure(int32_t adc_P) {
        int64_t var1 = ((int64_t)t_fine) - 128000;
        int64_t var2 = var1 * var1 * (int64_t)dig_P6;
        var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
        var2 = var2 + (((int64_t)dig_P4) << 35);
        var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
        var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
        if (var1 == 0) return 0;
        int64_t p = 1048576 - adc_P;
        p = (((p << 31) - var2) * 3125) / var1;
        var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
        var2 = (((int64_t)dig_P8) * p) >> 19;
        p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
        // 64-битная формула возвращает p в единицах 1/256 Па.
        // Делим на 256, чтобы получить Па (по даташиту Bosch/Adafruit).
        return (float)(p / 256.0);
    }
};

#endif // BMP280_H
