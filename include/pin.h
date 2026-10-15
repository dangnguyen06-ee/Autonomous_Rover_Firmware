#pragma once
#include <stdint.h>

namespace Pins {
    // SPI pins for BNO08x sensor
    constexpr uint8_t BNO08X_SCK = 12;   // SCL  (FSPICLK)
    constexpr uint8_t BNO08X_MISO = 13;  // SDA  (FSPIQ)
    constexpr uint8_t BNO08X_MOSI = 11;  // AD0  (FSPID)
    constexpr uint8_t BNO08X_CS = 10;    // (FSPICS0)
    constexpr uint8_t BNO08X_INT = 14;   // INT  (GPIO14)
    constexpr uint8_t BNO08X_RST = 9;    // RST  (GPIO9)

    // I2C pins for multiplexer and sensors
    constexpr uint8_t MUX_RESET_PIN = 4; 
    constexpr uint8_t SCL_PIN = 5;
    constexpr uint8_t SDA_PIN = 6;
    // i2c
}
TCA9548 tca9548(0x70);
uint8_t channels = 0;

Adafruit_VL53L5CX tof_l;    //  tof_l on channel 0
Adafruit_VL53L5CX tof_m;   //  tof_m on channel 1
Adafruit_VL53L5CX tof_r;   //  tof_r on channel 2
Adafruit_VEML7700 veml = Adafruit_VEML7700();  //  ambient light sensor on channel 3
