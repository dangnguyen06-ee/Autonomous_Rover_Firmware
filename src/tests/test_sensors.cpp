#include <Arduino.h>
#include <Wire.h>
#include <TCA9548.h>
#include <Adafruit_VL53L5CX.h>
#include <Adafruit_VEML7700.h>
#include <Adafruit_BNO08x.h>

#define MUX_RESET_PIN 4
#define SCL_PIN 5
#define SDA_PIN 6
// #define BNO08X_SCK  12   // SCL  (FSPICLK)
// #define BNO08X_MISO 13   // SDA  (FSPIQ)
// #define BNO08X_MOSI 11   // AD0  (FSPID)
// #define BNO08X_CS   10   // (FSPICS0)
// #define BNO08X_INT  14   // INT  (GPIO14)
// #define BNO08X_RST  9   // RST  (GPIO9)

// Adafruit_BNO08x bno08x(BNO08X_RST);
sh2_SensorValue_t sensorValue;
TCA9548 tca9548(0x70);
uint8_t channels = 0;

Adafruit_VL53L5CX tof_l;    //  tof_l on channel 0
Adafruit_VL53L5CX tof_m;   //  tof_m on channel 1
Adafruit_VL53L5CX tof_r;   //  tof_r on channel 2
Adafruit_VEML7700 veml = Adafruit_VEML7700();  //  ambient light sensor on channel 3

VL53L5CX_ResultsData results;

uint32_t lastTime = 0;

void halt(const __FlashStringHelper* msg) {
    Serial.println(msg);
    while (1)
        delay(10);
}

void setup() {
    Serial.begin(921600);
    delay(2000);
    Serial.println("boot");

    pinMode(MUX_RESET_PIN, OUTPUT);
    digitalWrite(MUX_RESET_PIN, LOW);
    delay(10);
    digitalWrite(MUX_RESET_PIN, HIGH);
    delay(10);

    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    //  initialize multiplexer
    if (tca9548.begin() == false){
        halt(F("Multiplexer error"));
    }
    channels = tca9548.channelCount();

    //  initialize the tof sensors
    Adafruit_VL53L5CX* sensors[3] = {&tof_l, &tof_m, &tof_r};
    for (uint8_t i = 0; i < 3; i++) {
        tca9548.selectChannel(i);
        Serial.printf("Initializing ToF %u on channel %u\n", i, i);
        if (!sensors[i]->begin(VL53L5CX_DEFAULT_ADDRESS, &Wire, 400000)) {
            Serial.printf("ToF %u init failed\n", i);
            halt(F("halting"));
        }
        sensors[i]->setResolution(64);
        sensors[i]->setRangingFrequency(15);
        sensors[i]->startRanging();
    }
    //  initialize the ambient light sensor
    tca9548.selectChannel(3);  //  ambient light sensor is on channel 3
    Serial.println("Initializing ambient light sensor");
    if (!veml.begin()) halt(F("Ambient light sensor error"));

    // //  initialize the BNO08x sensor
    // Serial.println("Initializing BNO08x sensor");
    // SPI.begin(BNO08X_SCK, BNO08X_MISO, BNO08X_MOSI, BNO08X_CS);
    // if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, 3000000)) {
    //     halt(F("BNO08x init failed"));
    // }
    // Serial.println("BNO08x initialized");
    // bno08x.enableReport(SH2_GAME_ROTATION_VECTOR);
}
// void bno08xReadAndPrint() {
//     if (bno08x.wasReset()) {
//         bno08x.enableReport(SH2_GAME_ROTATION_VECTOR, 10000); // 1 Hz
//     }

//     if (bno08x.getSensorEvent(&sensorValue) &&
//     sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
//         Serial.printf("%.4f,%.4f,%.4f,%.4f\n",
//                   sensorValue.un.gameRotationVector.real,
//                   sensorValue.un.gameRotationVector.i,
//                   sensorValue.un.gameRotationVector.j,
//                   sensorValue.un.gameRotationVector.k);
//     }
// }

void tofReadAndPrint(Adafruit_VL53L5CX &tof, uint8_t channel, const char* prefix) {
    tca9548.selectChannel(channel);
    if (!tof.isDataReady()) return;
    if (!tof.getRangingData(&results)) return;

    Serial.print(prefix);
    Serial.print(":");
    
    uint8_t totalPixels = (tof.getResolution() == 16) ? 16 : 64;
    for (uint8_t i = 0; i < totalPixels; i++) {
        Serial.print(results.distance_mm[i]);
        if (i < totalPixels - 1) {
            Serial.print(",");
        }
    }
    Serial.println();
}

void printAmbientLight() {
    tca9548.selectChannel(3);  //  ambient light sensor is on channel 3
    float lux = veml.readLux(VEML_LUX_AUTO);
    Serial.print("Ambient light: ");
    Serial.print(lux);
    Serial.println(" lux");
    Serial.println("Settings used for reading:");
    Serial.print(F("Gain: "));
    switch (veml.getGain()) {
        case VEML7700_GAIN_1: Serial.println("1"); break;
        case VEML7700_GAIN_2: Serial.println("2"); break;
        case VEML7700_GAIN_1_4: Serial.println("1/4"); break;
        case VEML7700_GAIN_1_8: Serial.println("1/8"); break;
    }
    Serial.print(F("Integration Time (ms): "));
    switch (veml.getIntegrationTime()) {
        case VEML7700_IT_25MS: Serial.println("25"); break;
        case VEML7700_IT_50MS: Serial.println("50"); break;
        case VEML7700_IT_100MS: Serial.println("100"); break;
        case VEML7700_IT_200MS: Serial.println("200"); break;
        case VEML7700_IT_400MS: Serial.println("400"); break;
        case VEML7700_IT_800MS: Serial.println("800"); break;
    }
}

// Optimized Non-Blocking ESP32 Loop
uint32_t lastIMU = 0;
uint32_t lastToF = 0;
uint32_t lastLux = 0;

void loop() {
    uint32_t now = millis();

    // // 1. BNO085 IMU @ 50 Hz (every 20 ms) -> Buttery Smooth 60 FPS
    // if (now - lastIMU >= 20) {
    //     lastIMU = now;
    //     if (bno08x.getSensorEvent(&sensorValue) && sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {
    //         Serial.printf("BNO:%.4f,%.4f,%.4f,%.4f\n",
    //             sensorValue.un.gameRotationVector.real,
    //             sensorValue.un.gameRotationVector.i,
    //             sensorValue.un.gameRotationVector.j,
    //             sensorValue.un.gameRotationVector.k);
    //     }
    // }

    // 2. VL53L5CX ToF Sensors @ 15 Hz (every 66 ms)
    if (now - lastToF >= 66) {
        lastToF = now;
        tofReadAndPrint(tof_l, 0, "TOFL");
        tofReadAndPrint(tof_m, 1, "TOFM");
        tofReadAndPrint(tof_r, 2, "TOFR");
    }

    // 3. VEML7700 Lux @ 2 Hz (every 500 ms)
    if (now - lastLux >= 500) {
        lastLux = now;
        tca9548.selectChannel(3);
        float lux = veml.readLux(VEML_LUX_AUTO);
        Serial.printf("LUX:%.2f\n", lux);
    }
}