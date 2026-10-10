#pragma once
#include <stdint.h>

constexpr uint16_t LUX_PERIOD_MS = 1000;  // how often to read the lux value from the VEML7700 sensor (in milliseconds)
constexpr uint16_t TOF_PERIOD_MS = 100;   // how often to read the distance values from the VL53L5CX sensors (in milliseconds)
constexpr uint16_t ESTOP_MM = 200;        // distance threshold for emergency stop (in millimeters)
constexpr uint16_t EstopTimeout = 1000;  // time threshold for emergency stop (in milliseconds)