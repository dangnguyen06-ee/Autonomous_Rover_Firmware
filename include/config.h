#pragma once
#include <stdint.h>

namespace Config {
    // I2C multiplexer address, number of sensors
    constexpr uint8_t TCA9548_ADDR = 0x70;
    constexpr uint8_t NUM_TOF_SENSORS = 3;
    constexpr uint8_t NUM_LUX_SENSORS = 1;
    constexpr uint8_t NUM_MUX_CHANNELS = 4;

    // Sensor channel mapping
    constexpr uint8_t MuxChannelTofLeft = 0; //  tof_l on channel 0
    constexpr uint8_t MuxChannelTofMiddle = 1; //  tof_m on channel 1
    constexpr uint8_t MuxChannelTofRight = 2; //  tof_r on channel 2
    constexpr uint8_t MuxChannelLux = 3; //  lux on channel 3

    // 
}


constexpr uint16_t LUX_PERIOD_MS = 1000;  // how often to read the lux value from the VEML7700 sensor (in milliseconds)
constexpr uint16_t TOF_PERIOD_MS = 100;   // how often to read the distance values from the VL53L5CX sensors (in milliseconds)
constexpr uint16_t ESTOP_MM = 200;        // distance threshold for emergency stop (in millimeters)
constexpr uint16_t EstopTimeout = 1000;  // time threshold for emergency stop (in milliseconds)
