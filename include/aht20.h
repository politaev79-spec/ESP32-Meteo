#ifndef AHT20_H
#define AHT20_H

#include <Arduino.h>
#include <Wire.h>

// ===========================================================================
//  AHT20 driver (temperature + humidity, I2C, native, no external library).
//  Address 0x38.
// ===========================================================================

class AHT20 {
public:
    bool begin(uint8_t addr = 0x38) {
        _addr = addr;
        Wire.beginTransmission(_addr);
        if (Wire.endTransmission() != 0) return false;
        // Init command (normal mode, enable calibration).
        Wire.beginTransmission(_addr);
        Wire.write(0xBE);
        Wire.write(0x08);
        Wire.write(0x00);
        if (Wire.endTransmission() != 0) return false;
        delay(50);
        return true;
    }

    // Returns true and fills temperature (C) and humidity (%RH).
    bool read(float &temp, float &hum) {
        Wire.beginTransmission(_addr);
        Wire.write(0xAC);   // trigger measurement
        Wire.write(0x33);
        Wire.write(0x00);
        if (Wire.endTransmission() != 0) return false;

        delay(80);
        // Read status until busy clears (bit7 = busy).
        for (int i = 0; i < 10; i++) {
            if (Wire.requestFrom((int)_addr, (int)1) == 1) {
                uint8_t st = Wire.read();
                if ((st & 0x80) == 0) break;
            }
            delay(10);
        }

        if (Wire.requestFrom((int)_addr, (int)6) != 6) return false;
        uint8_t b[6];
        for (int i = 0; i < 6; i++) b[i] = Wire.read();

        uint32_t hum_raw = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | ((uint32_t)b[3] >> 4);
        uint32_t temp_raw = (((uint32_t)b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | (uint32_t)b[5];

        hum = hum_raw * 100.0f / 1048576.0f;
        temp = temp_raw * 200.0f / 1048576.0f - 50.0f;
        return true;
    }

private:
    uint8_t _addr = 0x38;
};

#endif // AHT20_H
