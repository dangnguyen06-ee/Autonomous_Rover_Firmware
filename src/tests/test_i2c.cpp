#include <Arduino.h>
#include <Wire.h>
#include <TCA9548.h>
#include <Adafruit_VL53L5CX.h>
#include <Adafruit_VEML7700.h>

#define MUX_RESET_PIN 4
#define SCL_PIN 5
#define SDA_PIN 6

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
    Serial.begin(115200);
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
}

void tofReadAndPrint(Adafruit_VL53L5CX &tof, uint8_t channel) {
    tca9548.selectChannel(channel);
    if (!tof.isDataReady()) return;
    if (!tof.getRangingData(&results)) return;

    uint8_t width = (tof.getResolution() == 16) ? 4 : 8;
    for (int x = width - 1; x >= 0; x--) {
        for (int y = width * (width - 1); y >= 0; y -= width) {
            Serial.print('\t');
            Serial.print(results.distance_mm[x + y]);
        }
        Serial.println();
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

void loop() 
{
    if ((millis() - lastTime) > 100) //print every 100ms
    {
    lastTime = millis();

    tofReadAndPrint(tof_l, 0);
    tofReadAndPrint(tof_m, 1);
    tofReadAndPrint(tof_r, 2);
    printAmbientLight();
    }
}
