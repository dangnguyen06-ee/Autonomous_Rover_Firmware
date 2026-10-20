#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BNO08x.h>

#define BNO08X_SCK  12
#define BNO08X_MISO 13
#define BNO08X_MOSI 11
#define BNO08X_CS   10
#define BNO08X_INT  14
#define BNO08X_RST  9
#define BNO08X_WAKE 8   // PS0 (PS1 tied to 3.3V)

Adafruit_BNO08x bno08x(-1);  // reset handled manually in setup()
sh2_SensorValue_t sensorValue;

// Pull PS0/WAKE low until the BNO08x pulls INT low, so it will accept a command
bool wakeSensor() {
  digitalWrite(BNO08X_WAKE, LOW);
  uint32_t t0 = millis();
  while (digitalRead(BNO08X_INT) && millis() - t0 < 500) {}
  digitalWrite(BNO08X_WAKE, HIGH);
  return !digitalRead(BNO08X_INT);
}

void enableReports() {
  if (wakeSensor()) bno08x.enableReport(SH2_GAME_ROTATION_VECTOR);
  else Serial.println("BNO08x wake failed");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  pinMode(BNO08X_WAKE, OUTPUT);
  digitalWrite(BNO08X_WAKE, HIGH);  // must be HIGH during reset for SPI mode

  pinMode(BNO08X_RST, OUTPUT);
  digitalWrite(BNO08X_RST, LOW);  delay(10);
  digitalWrite(BNO08X_RST, HIGH); delay(500);

  SPI.begin(BNO08X_SCK, BNO08X_MISO, BNO08X_MOSI, BNO08X_CS);
  if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI)) {
    Serial.println("BNO08x init failed");
    while (1) delay(10);
  }
  enableReports();
}

void loop() {
  if (bno08x.wasReset()) enableReports();

  if (bno08x.getSensorEvent(&sensorValue) &&
      sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
    auto &q = sensorValue.un.gameRotationVector;
    Serial.printf("%.4f,%.4f,%.4f,%.4f\n", q.real, q.i, q.j, q.k);
  }
}