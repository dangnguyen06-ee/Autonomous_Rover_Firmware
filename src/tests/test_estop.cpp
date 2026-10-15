#include Arduino.h
#include <BTS7960.h>
#include <TCA9548.h>
#include <Adafruit_VL53L5CX.h>
#include <Adafruit_VEML7700.h>
#include <Wire.h>


//define MUX_reset pin, SCL pin and SDA pin
#define MUX_RESET_PIN 4
#define SCL_PIN 5
#define SDA_PIN 6

//define motor driver pins
#define L_EN 41
#define R_EN 42
#define L_PWM 1
#define R_PWM 2

BTS7960 motor1(L_EN, R_EN, L_PWM, R_PWM); //create an object of the class BTS7960

TCA9548 tca9548(0x70);
uint8_t channels = 0;

Adafruit_VL53L5CX tof_l;    //  tof_l on channel 0
Adafruit_VL53L5CX tof_m;   //  tof_m on channel 1
Adafruit_VL53L5CX tof_r;   //  tof_r on channel 2
Adafruit_VEML7700 veml = Adafruit_VEML7700();  //  ambient light sensor on channel 3

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
    
    //  initialize the motor driver
    Serial.println("BTS7960 Motor Driver Test");
    motor1.begin();                           //This method will set the motor driver pins as output
    Serial.println("Motor driver pins set as output");
    motor1.enable();                          //This method will set the L_EN and R_EN to HIGH or digitalWrite them to +5v/3v depending on your mcu
    Serial.println("Motor driver enabled");

}
