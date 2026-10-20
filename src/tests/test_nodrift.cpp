#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_BNO08x.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BTS7960.h>

// ============================================================
//  PINS
// ============================================================
// Motor driver 1 pins
#define M_EN 40 // Enable pin for both drivers (wired together)
#define L_RPWM 1
#define L_LPWM 2
 
// Motor driver 2 pins
#define R_RPWM 42
#define R_LPWM 41

// BNO085 sensor pins
#define BNO08X_SCK  12   // SCL  (FSPICLK)
#define BNO08X_MISO 13   // SDA  (FSPIQ)
#define BNO08X_MOSI 11   // AD0  (FSPID)
#define BNO08X_CS   10   // (FSPICS0)
#define BNO08X_INT  14   // INT  (GPIO14)
#define BNO08X_RST  9   // RST  (GPIO9)

// ============================================================
//  SETTINGS
// ============================================================
 
const char* WIFI_NAME = "Rover";
const char* WIFI_PASSWORD = "rover1234";  // at least 8 characters - change it!


const bool     LEFT_REVERSE  = false;      // flip if the left side drives backwards
const bool     RIGHT_REVERSE = false;      // flip if the right side drives backwards 
const uint8_t DEFAULT_SPEED_PERCENT = 20;  // speed when the rover boots (0-100)
// Motors on opposite sides of the rover usually face opposite ways, so one
// of them often needs reversing to make both wheels drive forward.
const bool MOTOR1_REVERSE = false;         // false = front(), true = back()
const bool MOTOR2_REVERSE = false;
 
const uint32_t CONTROL_TIMEOUT_MS = 2000;  // stop if the phone goes quiet this long

// Heading controller (PD). Tunable live from the web page.
float Kp = 3.0f;      // PWM counts per degree of error
float Kd = 0.4f;      // PWM counts per (degree/second)
const float MAX_CORRECTION = 100.0f;  // max PWM counts added/removed per side

// If the rover steers the WRONG way when it drifts, flip this to -1.
// (Yaw here is positive counter-clockwise with the sensor's Z axis pointing up.)
const float STEER_SIGN = 1.0f;

// ============================================================
//  GLOBALS
// ============================================================
Adafruit_BNO08x bno08x(BNO08X_RST);
sh2_SensorValue_t sensorValue;

BTS7960 lmotor(M_EN, M_EN, L_RPWM, L_LPWM); // left driver
BTS7960 rmotor(M_EN, M_EN, R_RPWM, R_LPWM); // right driver
WebServer server(80);
 
bool motorOn = false;                    // Start pressed on the phone
uint8_t speedPercent = DEFAULT_SPEED_PERCENT;
bool lostConnection = false;             // the phone-timeout switched the motor off
uint32_t lastContactMs = 0;
 
float    yawDeg = 0;          // current heading
bool     yawValid = false;
float    targetYaw = 0;       // heading to hold
float    lastError = 0;
uint32_t lastImuUs = 0;
int      leftPwm = 0, rightPwm = 0;

// ============================================================
//  HELPERS
// ============================================================
uint8_t percentToPwm(uint8_t percent) {
    return (uint16_t)percent * 255 / 100;
}

// wraps an angle to the range -180..180 degrees
float wrap180(float a) {
    while (a > 180.0f) a -= 360.0f;
    while (a < -180.0f) a += 360.0f;
    return a;
}

// Converts a quaternion to a yaw angle in degrees, using the right-hand rule with Z up.
float quatToYawDeg(float w, float x, float y, float z) {
    return atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z)) * 180.0f / PI;
}

// Drives one driver: pwm 0 = stop, otherwise spin in its configured direction
void driveSide(BTS7960& m, int pwm, bool reverse) {
    pwm = constrain(pwm, 0, 255);
    if (pwm == 0) { m.stop(); return; }
    m.pwm = pwm;
    if (reverse) m.back(); else m.front();
}

// Sets the left and right motor speeds (0-255). Stops if either is 0.
void setDrive(int left, int right) {
    leftPwm = constrain(left, 0, 255);
    rightPwm = constrain(right, 0, 255);
    driveSide(lmotor, leftPwm, LEFT_REVERSE);
    driveSide(rmotor, rightPwm, RIGHT_REVERSE);
}

void disableMotors() {
    lmotor.disable();
    rmotor.disable();
}

void stopAll() {
    lmotor.stop();
    rmotor.stop();
}

void halt(const __FlashStringHelper* msg) {
    stopAll();
    Serial.println(msg);
    while (1)
        delay(10);
}

// ============================================================
//  IMU + HEADING CONTROL
// ============================================================
// Runs the controller once per new IMU sample.
void runController(float dt) {
    float error = wrap180(targetYaw - yawDeg);
    float dError = (dt > 0.0f) ? wrap180(error - lastError) / dt : 0.0f;
    lastError = error;

    float corr = Kp * error + Kd * dError;
    corr = constrain(corr, -MAX_CORRECTION, MAX_CORRECTION);

    int base = percentToPwm(speedPercent);
    // error > 0 means we are to the right of the target, 
    // so we need to steer left (reduce left speed, increase right speed)
    setDrive(base - (int)corr, base + (int)corr);
}

// Reads the IMU and runs the controller if a new sample is available. Also handles re-enabling the IMU if it was reset.
void bno08xUpdate() {
    if (bno08x.wasReset()) {
        bno08x.enableReport(SH2_GAME_ROTATION_VECTOR, 10000); // 100 Hz
    }

    if (!bno08x.getSensorEvent(&sensorValue)) return;
    if (sensorValue.sensorId != SH2_GAME_ROTATION_VECTOR) return;

    yawDeg = quatToYawDeg(sensorValue.un.gameRotationVector.real,
                            sensorValue.un.gameRotationVector.i,
                            sensorValue.un.gameRotationVector.j,
                            sensorValue.un.gameRotationVector.k);
    yawValid = true;

    // Compute the time since the last IMU sample in seconds
    uint32_t now = micros();
    float dt = (lastImuUs == 0) ? 0.0f : (now - lastImuUs) / 1e6f;
    lastImuUs = now;
    
    // Run the controller if the motors are on
    if (motorOn) runController(dt);
}

// ============================================================
//  WEB PAGE
// ============================================================
const char PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Rover</title>
<style>
  body { font-family: sans-serif; max-width: 420px; margin: 20px auto; padding: 0 12px; }
  button { font-size: 1.4em; padding: 14px; width: 48%; border: 0; border-radius: 8px; color: #fff; }
  #go { background: #2a9d3a; } #stop { background: #c62828; }
  input[type=range] { width: 100%; }
  input[type=number] { width: 80px; }
  td { padding: 2px 8px; } table { margin-top: 12px; }
  #msg { color: #c62828; min-height: 1.2em; }
</style></head><body>
<h2>Rover straight-line</h2>
<div>
  <button id="go" onclick="go()">START</button>
  <button id="stop" onclick="stp()">STOP</button>
</div>
<p>Speed: <b id="sv">20</b>%<br>
<input type="range" id="speed" min="0" max="100" value="20"
       oninput="sv.textContent=this.value" onchange="send()"></p>
<p>Kp <input type="number" id="kp" step="0.1" value="3.0" onchange="send()">
   Kd <input type="number" id="kd" step="0.1" value="0.4" onchange="send()"></p>
<div id="msg"></div>
<table>
  <tr><td>State</td><td id="st">-</td></tr>
  <tr><td>Heading</td><td id="yaw">-</td></tr>
  <tr><td>Target</td><td id="tgt">-</td></tr>
  <tr><td>Error</td><td id="err">-</td></tr>
  <tr><td>Left PWM</td><td id="l">-</td></tr>
  <tr><td>Right PWM</td><td id="r">-</td></tr>
</table>
<script>
const $ = id => document.getElementById(id);
async function call(url) {
  try { const r = await fetch(url); return await r.json(); }
  catch (e) { $('msg').textContent = 'No connection'; return null; }
}
function send() { call('/set?speed=' + $('speed').value + '&kp=' + $('kp').value + '&kd=' + $('kd').value); }
async function go()  { send(); const s = await call('/start'); if (s && !s.imu) $('msg').textContent = 'IMU not ready'; }
function stp() { call('/stop'); }
async function poll() {
  const s = await call('/status');
  if (s) {
    $('msg').textContent = s.lost ? 'Stopped: connection lost' : '';
    $('st').textContent = s.on ? 'RUNNING' : 'stopped';
    $('yaw').textContent = s.yaw.toFixed(1) + '\u00B0';
    $('tgt').textContent = s.tgt.toFixed(1) + '\u00B0';
    $('err').textContent = s.err.toFixed(1) + '\u00B0';
    $('l').textContent = s.l; $('r').textContent = s.r;
  }
}
setInterval(poll, 250);   // also acts as the heartbeat
</script></body></html>
)HTML";
 
void touch() {
    lastContactMs = millis();
    lostConnection = false;
}
 
void handleRoot() {
    touch();
    server.send_P(200, "text/html", PAGE);
}
 
void handleSet() {
    touch();
    if (server.hasArg("speed")) speedPercent = constrain(server.arg("speed").toInt(), 0, 100);
    if (server.hasArg("kp")) Kp = server.arg("kp").toFloat();
    if (server.hasArg("kd")) Kd = server.arg("kd").toFloat();
    server.send(200, "application/json", "{\"ok\":true}");
}
 
void handleStart() {
    touch();
    if (yawValid) {
        targetYaw = yawDeg;     // hold whatever direction we're facing right now
        lastError = 0;
        lastImuUs = 0;
        motorOn = true;
    }
    server.send(200, "application/json", String("{\"imu\":") + (yawValid ? "true" : "false") + "}");
}
 
void handleStop() {
    touch();
    motorOn = false;
    stopAll();
    server.send(200, "application/json", "{\"ok\":true}");
}
 
void handleStatus() {
    touch();
    float err = wrap180(targetYaw - yawDeg);
    char buf[192];
    snprintf(buf, sizeof(buf),
             "{\"on\":%s,\"lost\":%s,\"imu\":%s,\"yaw\":%.2f,\"tgt\":%.2f,\"err\":%.2f,\"l\":%d,\"r\":%d}",
             motorOn ? "true" : "false", lostConnection ? "true" : "false",
             yawValid ? "true" : "false", yawDeg, targetYaw, motorOn ? err : 0.0f,
             leftPwm, rightPwm);
    server.send(200, "application/json", buf);
}

// ============================================================
//  SETUP / LOOP
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("boot");
 
    // Motors first, held stopped
    lmotor.begin();
    rmotor.begin();
    stopAll();
    lmotor.enable();
    rmotor.enable();
    Serial.println("Motors stopped, drivers enabled");

    //  initialize the BNO08x sensor
    Serial.println("Initializing BNO08x sensor");
    SPI.begin(BNO08X_SCK, BNO08X_MISO, BNO08X_MOSI, BNO08X_CS);
    if (!bno08x.begin_SPI(BNO08X_CS, BNO08X_INT, &SPI, 1000000)) {
        while (1) delay(10);
    }
    Serial.println("BNO08x initialized");
    Serial.println("Enabling BNO08x report");
    if (!bno08x.enableReport(SH2_GAME_ROTATION_VECTOR)) {
        halt(F("Failed to enable BNO08x report"));
    }

    // WiFi
    Serial.println("Starting WiFi");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(WIFI_NAME, WIFI_PASSWORD);
    Serial.print("Connect to WiFi network \"");
    Serial.print(WIFI_NAME);
    Serial.print("\" with password \"");
    Serial.print(WIFI_PASSWORD);
    Serial.println("\"");
    Serial.print("then open http://");
    Serial.print(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/set", handleSet);
    server.on("/start", handleStart);
    server.on("/stop", handleStop);
    server.on("/status", handleStatus);
    server.begin();
    Serial.println("Web server started");
}

void loop() {
    server.handleClient();
    bno08xUpdate();

    // If the phone hasn't contacted us for a while, stop the motors
    if (motorOn && millis() - lastContactMs > CONTROL_TIMEOUT_MS) {
        lostConnection = true;
        motorOn = false;
        stopAll();
    }
}