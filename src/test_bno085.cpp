#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BNO08x.h>

#define BNO08X_SCK  12   // SCL  (FSPICLK)
#define BNO08X_MISO 13   // SDA  (FSPIQ)
#define BNO08X_MOSI 11   // AD0  (FSPID)
#define BNO08X_CS   10   // (FSPICS0)
#define BNO08X_INT  14   // INT  (GPIO14)
#define BNO08X_RST  9   // RST  (GPIO9)

Adafruit_BNO08x bno08x(BNO08X_RST);
sh2_SensorValue_t sensorValue;

void setup() {
  Serial.begin(115200);
  SPI.begin(BNO08X_SCK, BNO08X_MISO, BNO08X_MOSI, BNO08X_CS);

  if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, 1000000)) {
    while (1) delay(10);
  }
  bno08x.enableReport(SH2_GAME_ROTATION_VECTOR);
}

void loop() {
  if (bno08x.wasReset()) {
    bno08x.enableReport(SH2_GAME_ROTATION_VECTOR);
  }

  if (bno08x.getSensorEvent(&sensorValue) &&
      sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
    Serial.printf("%.4f,%.4f,%.4f,%.4f\n",
                  sensorValue.un.gameRotationVector.real,
                  sensorValue.un.gameRotationVector.i,
                  sensorValue.un.gameRotationVector.j,
                  sensorValue.un.gameRotationVector.k);
  }
}