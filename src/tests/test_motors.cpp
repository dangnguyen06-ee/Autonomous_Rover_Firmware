#include <BTS7960.h>
#include <Arduino.h>
//BTS7960 works between +3v and +5v so you can use a uno, mega or a nano.
#define L_EN 41
#define R_EN 41
#define R_PWM 1
#define L_PWM 2

BTS7960 motor1(L_EN, R_EN, L_PWM, R_PWM);   //This method will create a object of the class BTS7960

bool stopped = false;

void checkSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 's') {
      motor1.stop();
      motor1.disable();
      stopped = true;
      Serial.println("Stopped");
    } else if (c == 'r') {
      motor1.enable();
      stopped = false;
      Serial.println("Resumed");
    }
  }
}

// replaces delay(): keeps listening for commands while waiting
void wait(unsigned long ms) {
  unsigned long t = millis();
  while (millis() - t < ms && !stopped) {
    checkSerial();
    delay(10);
  }
}

void setup() {
    Serial.begin(115200);  
    delay(1000);//begin the serial monitor for output

    Serial.println("BTS7960 Motor Driver Test");
    motor1.begin();                           //This method will set the motor driver pins as output
    Serial.println("Motor driver pins set as output");
    motor1.enable();                          //This method will set the L_EN and R_EN to HIGH or digitalWrite them to +5v/3v depending on your mcu
    Serial.println("Motor driver enabled");
}

void loop() {
       if (stopped) { checkSerial(); delay(10); return; }
    
    Serial.println("Motor running in clockwise direction");   
    for(int i=0; i<=255; i=i+10)
    {
        motor1.pwm = i;                         //Set the speed, by default the speed is set to 255 you can change it 
        motor1.front();                         //front functions should turn the motor in clockwise direction
        wait(500);
    }

    Serial.println("Motor stopped temporarily");
    motor1.stop();                            //stop function should halt the motor by applying 0 pwm to both R_PWM and L_PWM
    wait(500);

    Serial.println("Motor running in anti-clockwise direction");
    for(int i=0; i<=255; i=i+10)
    { 
        motor1.pwm = i;                         //Set the speed
        motor1.back();                          //back functions should turn the motor in anti-clockwise direction
        wait(500);
    }

    Serial.println("Motor stopped");
    wait(500);
    motor1.disable();                         //This method will set the L_EN and R_EN to LOW or digitalWrite them to 0v
}
