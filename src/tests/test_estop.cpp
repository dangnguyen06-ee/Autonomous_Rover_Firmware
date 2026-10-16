/*
 * test_estop.cpp
 *
 * E-stop test: the motor runs while all three ToF sensors (L / M / R) are
 * clear, and stops as soon as ANY one of them sees something closer than
 * STOP_DISTANCE_MM. It starts again once everything is farther than
 * RESUME_DISTANCE_MM.
 *
 * Serial Monitor commands (just type the letter):
 *   s = manual stop (motor stays off until you send r)
 *   r = resume automatic control
 *
 * Motor pins and commands are the same as test_motors.cpp.
 *
 * Stopping also turns the driver's enable pins OFF (motor1.disable()), so the
 * motor stops even if a PWM wire is loose. That's the same thing the 's'
 * command in test_motors.cpp does.
 */
 
#include <Arduino.h>
#include <BTS7960.h>
#include <TCA9548.h>
#include <Adafruit_VL53L5CX.h>
#include <Adafruit_VEML7700.h>
#include <Wire.h>
 
// MUX reset, SCL and SDA pins
#define MUX_RESET_PIN 15
#define SCL_PIN 5
#define SDA_PIN 6
 
// Motor driver pins - same as test_motors.cpp
#define L_EN 41
#define R_EN 41
#define R_PWM 1
#define L_PWM 2
 
// ============================================================
//  SETTINGS
// ============================================================
 
const int16_t STOP_DISTANCE_MM = 300;    // stop when anything is closer than this
const int16_t RESUME_DISTANCE_MM = 350;  // run again once everything is farther than this
const uint8_t MIN_ZONES = 3;             // zones (of 64) that must see it; 1 = most sensitive
 
const uint8_t MOTOR_SPEED = 150;    // 0-255 ONLY. No negatives - use MOTOR_REVERSE for direction
const bool MOTOR_REVERSE = false;  // false = front(), true = back()
 
const uint32_t SENSOR_TIMEOUT_MS = 500;  // stop if a sensor goes quiet this long
 
// ============================================================
 
BTS7960 motor1(L_EN, R_EN, L_PWM, R_PWM);
 
TCA9548 tca9548(0x70);
 
Adafruit_VL53L5CX tof_l;  // channel 0
Adafruit_VL53L5CX tof_m;  // channel 1
Adafruit_VL53L5CX tof_r;  // channel 2
Adafruit_VEML7700 veml = Adafruit_VEML7700();  // channel 3
 
const uint8_t NUM_TOF = 3;
Adafruit_VL53L5CX* sensors[NUM_TOF] = {&tof_l, &tof_m, &tof_r};  // index = mux channel
const char* sensorNames[NUM_TOF] = {"L", "M", "R"};
 
VL53L5CX_ResultsData results;  // shared buffer, processed right after each read
 
struct TofReading {
    bool hasData = false;        // false until the sensor delivers its first frame
    int16_t nearestMm = -1;      // closest valid distance, -1 = nothing seen
    uint8_t zonesInStop = 0;     // zones closer than STOP_DISTANCE_MM
    uint8_t zonesInResume = 0;   // zones closer than RESUME_DISTANCE_MM
    uint32_t lastUpdateMs = 0;
};
TofReading readings[NUM_TOF];
 
bool objectInRange = false;
bool motorRunning = false;
bool manualStop = false;
 
// ------------------------------------------------------------
//  Motor (same library calls as test_motors.cpp)
// ------------------------------------------------------------
 
void motorRun() {
    motor1.pwm = MOTOR_SPEED;
    if (MOTOR_REVERSE) {
        motor1.back();
    } else {
        motor1.front();
    }
    motor1.enable();   // turn the driver on only after the speed is set
}

void motorStop() {
    motor1.stop();
}
 
// Only talks to the driver when the state actually changes.
void setMotor(bool run, const char* reason) {
    if (run == motorRunning) return;
    if (run) {
        motorRun();
    } else {
        motorStop();
    }
    motorRunning = run;
    Serial.printf(">>> Motor %s (%s)\n", run ? "ON" : "OFF", reason);
}
 
void halt(const __FlashStringHelper* msg) {
    motor1.stop();
    motor1.disable();
    Serial.println(msg);
    while (1)
        delay(10);
}
 
// Manual stop / resume from the Serial Monitor, same idea as test_motors.cpp
void checkSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 's') {
            manualStop = true;
            motorStop();
            motorRunning = false;
            Serial.println(">>> MANUAL STOP - send 'r' to resume");
        } else if (c == 'r' && manualStop) {
            manualStop = false;  // loop() turns the motor back on if the path is clear
            Serial.println(">>> Resumed automatic control");
        }
    }
}
 
// ------------------------------------------------------------
//  ToF
// ------------------------------------------------------------
 
// Reads one sensor (if it has a new frame) and summarizes what it sees.
void readSensor(uint8_t i) {
    tca9548.selectChannel(i);
    if (!sensors[i]->isDataReady()) return;
    if (!sensors[i]->getRangingData(&results)) return;
 
    TofReading r;
    for (uint8_t z = 0; z < 64; z++) {
        if (results.nb_target_detected[z] == 0) continue;
 
        uint16_t t = z * VL53L5CX_NB_TARGET_PER_ZONE;
        uint8_t status = results.target_status[t];
        if (status != 5 && status != 9) continue;  // 5 and 9 = valid measurement
 
        int16_t d = results.distance_mm[t];
        if (r.nearestMm < 0 || d < r.nearestMm) r.nearestMm = d;
        if (d < STOP_DISTANCE_MM) r.zonesInStop++;
        if (d < RESUME_DISTANCE_MM) r.zonesInResume++;
    }
    r.hasData = true;
    r.lastUpdateMs = millis();
    readings[i] = r;
}
 
// ------------------------------------------------------------
 
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("boot");
 
    // Motor driver first, held stopped so nothing moves during sensor startup
    motor1.begin();
    motorStop();  // PWM 0 and driver disabled
    Serial.println("Motor driver pins set as output, driver disabled");
 
    pinMode(MUX_RESET_PIN, OUTPUT);
    digitalWrite(MUX_RESET_PIN, LOW);
    delay(100);
    digitalWrite(MUX_RESET_PIN, HIGH);
    delay(100);
 
    Serial.println("starting mux");
    Wire.begin(SDA_PIN, SCL_PIN, 400000);
    if (tca9548.begin() == false) {
        halt(F("Multiplexer error"));
    }
 
    // ToF sensors (each can take up to ~10 s on first power-up)
    for (uint8_t i = 0; i < NUM_TOF; i++) {
        tca9548.selectChannel(i);
        Serial.printf("Initializing ToF %s on channel %u\n", sensorNames[i], (unsigned)i);
 
        bool ok = sensors[i]->begin(VL53L5CX_DEFAULT_ADDRESS, &Wire, 400000) &&
                  sensors[i]->setResolution(64) &&
                  sensors[i]->setRangingFrequency(15) &&
                  sensors[i]->startRanging();
        if (!ok) {
            Serial.printf("ToF %s init failed\n", sensorNames[i]);
            halt(F("halting"));
        }
    }
 
    // Ambient light sensor
    tca9548.selectChannel(3);
    Serial.println("Initializing ambient light sensor");
    if (!veml.begin()) halt(F("Ambient light sensor error"));
 
    // The driver stays disabled here; motorRun() enables it when the path is clear
    Serial.println("Running. Type s = stop, r = resume\n");
}
 
void loop() {
    checkSerial();
 
    for (uint8_t i = 0; i < NUM_TOF; i++) {
        readSensor(i);
    }
 
    uint32_t now = millis();
    bool sensorsOk = true;
    bool nearNow = false;
 
    for (uint8_t i = 0; i < NUM_TOF; i++) {
        if (!readings[i].hasData || now - readings[i].lastUpdateMs > SENSOR_TIMEOUT_MS) {
            sensorsOk = false;
        }
        // Once stopped, wait until the object is past RESUME_DISTANCE_MM before
        // running again, so the motor doesn't stutter at the edge.
        uint8_t zones = objectInRange ? readings[i].zonesInResume : readings[i].zonesInStop;
        if (zones >= MIN_ZONES) {
            nearNow = true;  // any ONE sensor is enough to stop
        }
    }
    objectInRange = nearNow;
 
    if (!manualStop) {
        if (!sensorsOk) {
            setMotor(false, "waiting for sensors");
        } else if (objectInRange) {
            setMotor(false, "object in range");
        } else {
            setMotor(true, "path clear");
        }
    }
 
    // Status line 4x per second: nearest distance (zones in range) per sensor
    static uint32_t lastPrint = 0;
    if (now - lastPrint >= 250) {
        lastPrint = now;
        for (uint8_t i = 0; i < NUM_TOF; i++) {
            if (readings[i].nearestMm < 0) {
                Serial.printf("%s:   ---        | ", sensorNames[i]);
            } else {
                Serial.printf("%s: %4d mm (%2u) | ", sensorNames[i], readings[i].nearestMm,
                              (unsigned)readings[i].zonesInStop);
            }
        }
        const char* state = manualStop      ? "MANUAL STOP"
                            : !sensorsOk     ? "SENSOR WAIT/TIMEOUT"
                            : objectInRange  ? "OBJECT"
                                             : "clear";
        Serial.printf("%s | %s\n", state, motorRunning ? "MOTOR ON" : "motor off");
    }
 
    delay(5);
}