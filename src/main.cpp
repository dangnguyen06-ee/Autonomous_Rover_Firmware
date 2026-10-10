// src/main.cpp
void loop() {
    tof::update();
    light::update();     // rate-limits itself to 4 Hz
    imu::update();
    safety::update();    // reads tof/imu, may call motors::stop()
    comms::update();     // phone commands in, sensor lines out
}