#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

// Change these to match your wiring (8/9 are the S3's default I2C pins)
#define I2C_SDA 8
#define I2C_SCL 9

const int imageWidth = 8;  // 8x8 = 64 zones

SparkFun_VL53L5CX imager;
VL53L5CX_ResultsData data;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // sensor supports up to 1 MHz if your wires are short

  Serial.println("Starting VL53L5CX (firmware upload takes a few seconds)...");
  if (!imager.begin()) {
    Serial.println("VL53L5CX not found. Check SDA/SCL, power, and that LPn is high.");
    while (1) delay(10);
  }

  imager.setResolution(imageWidth * imageWidth);
  imager.setRangingFrequency(15);  // Hz, max 15 in 8x8 mode (60 in 4x4)
  imager.startRanging();
  Serial.println("Ranging started.");
}

void loop() {
  if (imager.isDataReady() && imager.getRangingData(&data)) {
    // Flipped so the grid matches what the sensor sees
    for (int y = 0; y <= imageWidth * (imageWidth - 1); y += imageWidth) {
      for (int x = imageWidth - 1; x >= 0; x--) {
        Serial.printf("%5d", data.distance_mm[x + y]);
      }
      Serial.println();
    }
    Serial.println();
  }
  delay(5);
}