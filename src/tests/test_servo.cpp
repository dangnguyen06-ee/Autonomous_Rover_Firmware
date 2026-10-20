#include <Arduino.h>
#include <ESP32Servo.h>

// Define the pin connected to your servo signal line (Orange)
#define SERVO_PIN 14

Servo myServo;

void setup() {
    // Initialize Serial Monitor just for startup messages
    Serial.begin(115200);
    delay(500); // Wait for USB CDC to enumerate

    Serial.println("\n--- MG90S Automatic Sweep Test ---");
    Serial.println("Sweeping from 0 to 180 and back...");

    // Allow flexible PWM allocation on ESP32
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    ESP32PWM::allocateTimer(3);
    
    // Set standard frequency for analog micro servos (50Hz)
    myServo.setPeriodHertz(50); 
    
    // Attach the servo pin along with min/max pulse widths for the MG90S
    myServo.attach(SERVO_PIN, 500, 2400); 
}

void loop() {
    // Sweep from 0 to 180 degrees
    myServo.write(0);
    delay(1000); // Hold at 0 degrees for 1 second
    myServo.write(90);
    delay(1000); // Hold at 90 degrees for 1 second
    myServo.write(180);
    delay(1000); // Hold at 180 degrees for 1 second
}