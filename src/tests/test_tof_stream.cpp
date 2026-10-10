// Streams the three VL53L5CX 8x8 grids over serial in a line format the HTML viewer can parse.
//
// Line format (one line per sensor frame):
//   L:d0,d1,...,d63      (also M: and R:)
// Distances are in mm, index = col + row*8 (the raw library order).
// -1 means "no valid target in this zone".
// Lines starting with '#' are status/log messages.

#include <Arduino.h>
#include <Wire.h>
#include <TCA9548.h>
#include <Adafruit_VL53L5CX.h>

TCA9548 tca9548(0x70);

const uint8_t NUM_SENSORS = 3;
Adafruit_VL53L5CX tof[NUM_SENSORS];            // channel 0 = left, 1 = middle, 2 = right
const char TAGS[NUM_SENSORS] = {'L', 'M', 'R'};

VL53L5CX_ResultsData results;

void halt(const __FlashStringHelper* msg) {
    Serial.print(F("# ERROR: "));
    Serial.println(msg);
    while (1)
        delay(10);
}

void setup() {
    Serial.begin(921600);                      // ignored on native USB, matters on the UART port
    uint32_t t0 = millis();
    while (!Serial && millis() - t0 < 2000)    // wait max 2 s, so it also runs without a PC
        delay(10);

    Serial.println(F("# VL53L5CX stream starting"));

    Wire.begin();                              // default pins (use Wire.begin(SDA, SCL) to change)
    if (!tca9548.begin())
        halt(F("Multiplexer not found"));

    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        tca9548.selectChannel(i);
        if (!tof[i].begin(VL53L5CX_DEFAULT_ADDRESS, &Wire, 400000))
            halt(F("Sensor init failed"));
        tof[i].setResolution(64);              // 8x8
        tof[i].setRangingFrequency(15);        // max 15 Hz at 8x8
        tof[i].startRanging();
        Serial.print(F("# sensor "));
        Serial.print(TAGS[i]);
        Serial.println(F(" ready"));
    }
}

void loop() {
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
        tca9548.selectChannel(i);
        if (!tof[i].isDataReady()) continue;
        if (!tof[i].getRangingData(&results)) continue;

        Serial.print(TAGS[i]);
        Serial.print(':');
        for (uint8_t z = 0; z < 64; z++) {
            uint8_t st = results.target_status[z];
            bool valid = (st == 5 || st == 9);  // 5 = valid, 9 = valid (lower confidence)
            Serial.print(valid ? (int)results.distance_mm[z] : -1);
            if (z < 63) Serial.print(',');
        }
        Serial.println();
    }
}