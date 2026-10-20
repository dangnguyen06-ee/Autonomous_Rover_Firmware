#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BNO08x.h>

#define BNO08X_SCK  12   // SCL  (FSPICLK)
#define BNO08X_MISO 13   // SDA  (FSPIQ)
#define BNO08X_MOSI 11   // AD0  (FSPID)
#define BNO08X_CS   10   // (FSPICS0)
#define BNO08X_INT  14   // INT  (GPIO14)
#define BNO08X_RST  9   // RST  (GPIO9)

// -1 = the library does NOT touch the reset pin; we reset the chip ourselves below
Adafruit_BNO08x bno08x(-1);
sh2_SensorValue_t sensorValue;

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Reset the BNO085 ourselves, then give it time to boot before the library talks to it
  pinMode(BNO08X_RST, OUTPUT);
  digitalWrite(BNO08X_RST, HIGH); delay(10);
  digitalWrite(BNO08X_RST, LOW);  delay(10);
  digitalWrite(BNO08X_RST, HIGH);
  delay(500);

  // Initialize the BNO08x sensor
  Serial.println("Initializing BNO08x sensor");
  SPI.begin(BNO08X_SCK, BNO08X_MISO, BNO08X_MOSI, BNO08X_CS);
  Serial.println("SPI initialized");
  
  if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, 1000000)) {
    Serial.println("BNO08x init failed");
    while (1) delay(10);
  }
  Serial.println("BNO08x initialized");
  Serial.println("Enabling BNO08x report");
  bno08x.enableReport(SH2_GAME_ROTATION_VECTOR);
  Serial.println("BNO08x report enabled");
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