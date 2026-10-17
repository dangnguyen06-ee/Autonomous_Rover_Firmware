/*
 * test_roverweb.cpp  -  MOTOR TEST ONLY (no sensors)
 *
 * Control the motor from your phone over Wi-Fi.
 *
 *   1. Upload this and wait for "Ready" in the Serial Monitor.
 *   2. On your phone, join the Wi-Fi network WIFI_NAME (password WIFI_PASSWORD).
 *   3. Open http://192.168.4.1 in the phone's browser.
 *      Drive mode: Start / Stop the motor.  Speed mode: set the motor speed.
 *   4. Optional: "Add to Home Screen" so it opens like an app.
 *
 * Safety:
 *   - The motor never starts on its own. Someone has to press Start motor.
 *   - If the phone stops talking to the rover for CONTROL_TIMEOUT_MS (screen
 *     locked, out of range, page closed), the motor switches off.
 *   - There is NO obstacle stop in this version. Keep the wheels off the ground.
 *
 * Two BTS7960 drivers, both driven at the same speed from the phone.
 */
 
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <BTS7960.h>
 
// Motor driver 1 pins
#define L_EN 4
#define R_EN 4
#define R_PWM 1
#define L_PWM 2
 
// Motor driver 2 pins
#define L1_EN 40
#define R1_EN 40
#define R1_PWM 42
#define L1_PWM 41
 
// ============================================================
//  SETTINGS
// ============================================================
 
const char* WIFI_NAME = "Rover";
const char* WIFI_PASSWORD = "rover1234";  // at least 8 characters - change it!
 
const uint8_t DEFAULT_SPEED_PERCENT = 20;  // speed when the rover boots (0-100)
// Motors on opposite sides of the rover usually face opposite ways, so one
// of them often needs reversing to make both wheels drive forward.
const bool MOTOR1_REVERSE = false;         // false = front(), true = back()
const bool MOTOR2_REVERSE = false;
 
const uint32_t CONTROL_TIMEOUT_MS = 2000;  // stop if the phone goes quiet this long
 
// ============================================================
 
BTS7960 motor1(L_EN, R_EN, L_PWM, R_PWM);       // driver 1
BTS7960 motor2(L1_EN, R1_EN, L1_PWM, R1_PWM);   // driver 2
WebServer server(80);
 
bool motorOn = false;                    // Start pressed on the phone
uint8_t speedPercent = DEFAULT_SPEED_PERCENT;
bool lostConnection = false;             // the phone-timeout switched the motor off
uint32_t lastContactMs = 0;
 
uint8_t appliedPwm = 0;                  // what the driver is actually getting
 
// ------------------------------------------------------------
//  Phone page (served at http://192.168.4.1)
// ------------------------------------------------------------
 
const char PAGE[] = R"PAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#000000">
<meta name="color-scheme" content="dark">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<meta name="apple-mobile-web-app-title" content="Parker">
<title>Parker</title>
<style>
:root{
  --bg:#000000; --surface:#15171A; --line:#2A2D31; --text:#F2F3F5; --muted:#9AA0A6;
  --stop:#2A2D32; --stop-edge:#454950; --go:#1E9E57; --go-deep:#147040;
  --alert:#FF6B63; --fill:#E6E8EB; --focus:#6EA8FE;
}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg)}
body{color:var(--text);font:16px/1.4 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;
  -webkit-tap-highlight-color:transparent;-webkit-user-select:none;user-select:none}
.app{max-width:480px;margin:0 auto;min-height:100vh;min-height:100dvh;
  padding:16px 16px calc(16px + env(safe-area-inset-bottom));
  display:flex;flex-direction:column;gap:14px}
header{display:flex;align-items:center;justify-content:space-between}
.brand{display:flex;align-items:center;gap:10px}
.brand svg{width:34px;height:34px;color:var(--text);flex:none}
h1{font-size:30px;font-weight:850;letter-spacing:-.03em;margin:0}
.link{display:flex;align-items:center;gap:8px;font-size:14px;font-weight:600;color:var(--muted)}
.dot{width:10px;height:10px;border-radius:50%;background:var(--muted)}
.link.ok .dot{background:var(--go)}
.link.bad{color:var(--alert)} .link.bad .dot{background:var(--alert)}
 
.status{background:var(--surface);border:1px solid var(--line);border-radius:12px;padding:14px 16px}
.state{font-size:23px;font-weight:850;letter-spacing:-.01em;margin:0}
.state.run{color:#3DD17F} .state.alert{color:var(--alert)}
.detail{color:var(--muted);font-size:15px;margin:2px 0 0;min-height:21px}
 
.controls{flex:1;display:grid;grid-template-columns:1fr 104px;gap:16px;min-height:380px}
.left{display:flex;flex-direction:column;gap:14px;min-width:0}
.buttons{margin-top:auto;display:flex;flex-direction:column;gap:10px}
.btn{width:100%;font:inherit;font-size:17px;font-weight:800;padding:13px 12px;border-radius:8px;
  cursor:pointer;transition:transform .08s,filter .08s}
.btn:active{transform:scale(.98);filter:brightness(1.2)}
.start{color:#fff;background:var(--go);border:2px solid var(--go)}
.start[aria-pressed="true"]{background:transparent;color:#3DD17F;border-color:var(--go-deep)}
.stop{color:var(--text);background:var(--stop);border:2px solid var(--stop-edge)}
 
.speed{display:flex;flex-direction:column;align-items:center;gap:8px}
.speed-value{font-size:28px;font-weight:850;font-variant-numeric:tabular-nums;line-height:1}
.speed-name{font-size:13px;color:var(--muted);margin-top:-4px}
.end{font-size:14px;font-weight:700;color:var(--muted)}
.vslider{flex:1;width:72px;min-height:220px;position:relative;touch-action:none;cursor:pointer;border-radius:12px}
.track{position:absolute;left:50%;top:14px;bottom:14px;width:22px;transform:translateX(-50%);
  background:var(--surface);border:1px solid var(--line);border-radius:11px;overflow:hidden}
.fill{position:absolute;left:0;right:0;bottom:0;height:20%;background:var(--fill)}
.thumb{position:absolute;left:50%;bottom:20%;width:56px;height:28px;transform:translate(-50%,50%);
  margin-bottom:0;background:var(--text);border:3px solid var(--bg);border-radius:8px;
  box-shadow:0 0 0 2px var(--fill)}
.thumb-rail{position:absolute;left:0;right:0;top:14px;bottom:14px;pointer-events:none}
 
.note{font-size:13px;color:var(--muted);margin:0;text-align:center}
button:focus-visible,.vslider:focus-visible{outline:3px solid var(--focus);outline-offset:3px}
@media (prefers-reduced-motion:reduce){*{transition:none!important}}
</style>
</head>
<body>
<main class="app">
  <header>
    <div class="brand">
      <svg viewBox="0 0 32 32" aria-hidden="true">
        <g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">
          <polyline points="12.5,12 7,7 5,2"/>   <polyline points="19.5,12 25,7 27,2"/>
          <polyline points="12,14.5 5,11 2,13"/> <polyline points="20,14.5 27,11 30,13"/>
          <polyline points="12,17 5,18 2,22"/>   <polyline points="20,17 27,18 30,22"/>
          <polyline points="12.5,19.5 7,25 5,30"/><polyline points="19.5,19.5 25,25 27,30"/>
        </g>
        <circle cx="16" cy="10.5" r="3" fill="currentColor"/>
        <ellipse cx="16" cy="19.5" rx="5" ry="6" fill="currentColor"/>
      </svg>
      <h1>Parker</h1>
    </div>
    <div class="link" id="link" role="status"><span class="dot"></span><span id="linkText">Connecting</span></div>
  </header>
 
  <section class="controls">
    <div class="left">
      <section class="status" aria-live="polite">
        <p class="state" id="state">Connecting to the rover</p>
        <p class="detail" id="detail"></p>
      </section>
      <div class="buttons">
        <button class="btn start" id="start" aria-pressed="false">Start motor</button>
        <button class="btn stop" id="stop">Stop</button>
      </div>
    </div>
 
    <div class="speed">
      <div class="speed-value"><span id="speedOut">20</span>%</div>
      <div class="speed-name">Speed</div>
      <div class="end">Max</div>
      <div class="vslider" id="vslider" role="slider" tabindex="0" aria-label="Motor speed"
           aria-orientation="vertical" aria-valuemin="0" aria-valuemax="100" aria-valuenow="20">
        <div class="track"><div class="fill" id="fill"></div></div>
        <div class="thumb-rail"><div class="thumb" id="thumb"></div></div>
      </div>
      <div class="end">0</div>
    </div>
  </section>
 
  <p class="note">The motor also stops if this page closes or the phone locks.</p>
</main>
 
<script>
const $ = id => document.getElementById(id);
const vslider = $('vslider');
let fails = 0, polling = false, holdSpeedUntil = 0, lastSpeedSent = 0;
let speed = 20, dragging = false;
 
async function call(path) {
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), 1500);
  try {
    const r = await fetch(path, {cache: 'no-store', signal: ctl.signal});
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const s = await r.json();
    fails = 0;
    render(s);
    return s;
  } catch (e) {
    if (++fails >= 3) render(null);
    throw e;
  } finally {
    clearTimeout(timer);
  }
}
 
async function poll() {
  if (polling) return;
  polling = true;
  try { await call('/status'); } catch (e) {} finally { polling = false; }
}
 
function setText(state, detail, cls) {
  $('state').textContent = state;
  $('state').className = 'state' + (cls ? ' ' + cls : '');
  $('detail').textContent = detail;
}
 
function showSpeed(v) {
  speed = v;
  $('speedOut').textContent = v;
  $('fill').style.height = v + '%';
  $('thumb').style.bottom = v + '%';
  vslider.setAttribute('aria-valuenow', v);
  vslider.setAttribute('aria-valuetext', v + '%');
}
 
function render(s) {
  const link = $('link');
  if (!s) {
    link.className = 'link bad';
    $('linkText').textContent = 'Not connected';
    setText('No connection to the rover', 'Join the Rover Wi-Fi network, then reload this page.', 'alert');
    return;
  }
  link.className = 'link ok';
  $('linkText').textContent = 'Connected';
 
  if (s.running)   setText('Running at ' + s.speed + '%', 'Press Stop to switch the motor off.', 'run');
  else if (s.on)   setText('Speed is 0%', 'Slide the speed up to move.', '');
  else if (s.lost) setText('Stopped', 'The phone lost contact, so the motor switched off. Press Start motor to continue.', 'alert');
  else             setText('Stopped', 'Press Start motor to run.', '');
 
  const start = $('start');
  start.setAttribute('aria-pressed', String(s.on));
  start.textContent = s.running ? 'Motor is running' : s.on ? 'Waiting to run' : 'Start motor';
 
  if (!dragging && Date.now() > holdSpeedUntil) showSpeed(s.speed);
}
 
function sendSpeed(v) {
  holdSpeedUntil = Date.now() + 1500;
  lastSpeedSent = Date.now();
  call('/set?speed=' + v).catch(() => {});
}
 
function setSpeedLocal(v, final) {
  v = Math.max(0, Math.min(100, Math.round(v)));
  holdSpeedUntil = Date.now() + 1500;
  if (v !== speed) showSpeed(v);
  if (final || Date.now() - lastSpeedSent > 200) sendSpeed(v);
}
 
// Vertical slider: drag anywhere on it, bottom = 0, top = Max
function valueFromY(y) {
  const r = vslider.getBoundingClientRect();
  const top = r.top + 14, bottom = r.bottom - 14;
  return (bottom - y) / (bottom - top) * 100;
}
vslider.addEventListener('pointerdown', e => {
  dragging = true;
  vslider.setPointerCapture(e.pointerId);
  setSpeedLocal(valueFromY(e.clientY), false);
});
vslider.addEventListener('pointermove', e => {
  if (dragging) setSpeedLocal(valueFromY(e.clientY), false);
});
const endDrag = e => {
  if (!dragging) return;
  dragging = false;
  setSpeedLocal(speed, true);
};
vslider.addEventListener('pointerup', endDrag);
vslider.addEventListener('pointercancel', endDrag);
vslider.addEventListener('keydown', e => {
  const step = {ArrowUp: 5, ArrowRight: 5, ArrowDown: -5, ArrowLeft: -5, PageUp: 20, PageDown: -20}[e.key];
  if (e.key === 'Home') { e.preventDefault(); setSpeedLocal(0, true); }
  else if (e.key === 'End') { e.preventDefault(); setSpeedLocal(100, true); }
  else if (step) { e.preventDefault(); setSpeedLocal(speed + step, true); }
});
 
async function stopNow() {
  for (let i = 0; i < 3; i++) {
    try { await call('/set?on=0'); return; } catch (e) {}
  }
}
$('start').addEventListener('click', () => call('/set?on=1').catch(() => {}));
$('stop').addEventListener('click', stopNow);
 
// Switching apps or locking the phone stops the motor right away
document.addEventListener('visibilitychange', () => {
  if (document.hidden && navigator.sendBeacon) navigator.sendBeacon('/set?on=0');
});
 
showSpeed(speed);
poll();
setInterval(poll, 300);
</script>
</body>
</html>
)PAGE";
 
// ------------------------------------------------------------
//  Motor
// ------------------------------------------------------------
 
uint8_t percentToPwm(uint8_t percent) {
    return (uint16_t)percent * 255 / 100;
}
 
// Drives one driver: pwm 0 = stop, otherwise spin in its configured direction
void driveOne(BTS7960& m, uint8_t pwm, bool reverse) {
    if (pwm == 0) {
        m.stop();  // both PWM pins to 0
        return;
    }
    m.pwm = pwm;
    if (reverse) {
        m.back();
    } else {
        m.front();
    }
}
 
void stopAll() {
    motor1.stop();
    motor2.stop();
}
 
// Only talks to the drivers when the output actually changes.
void applyMotor(uint8_t pwm) {
    if (pwm == appliedPwm) return;
    driveOne(motor1, pwm, MOTOR1_REVERSE);
    driveOne(motor2, pwm, MOTOR2_REVERSE);
    appliedPwm = pwm;
    Serial.printf(">>> Motors %s (PWM %u)\n", pwm ? "ON" : "OFF", (unsigned)pwm);
}
 
// Runs only when Start is on and speed > 0
void updateMotor() {
    bool run = motorOn && speedPercent > 0;
    applyMotor(run ? percentToPwm(speedPercent) : 0);
}
 
void halt(const __FlashStringHelper* msg) {
    stopAll();
    Serial.println(msg);
    while (1)
        delay(10);
}
 
// ------------------------------------------------------------
//  Web server
// ------------------------------------------------------------
 
void sendStatus() {
    char json[120];
    snprintf(json, sizeof(json),
             "{\"on\":%s,\"running\":%s,\"speed\":%u,\"lost\":%s}",
             motorOn ? "true" : "false",
             appliedPwm > 0 ? "true" : "false",
             (unsigned)speedPercent,
             lostConnection ? "true" : "false");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
}
 
void handleRoot() {
    server.send(200, "text/html", PAGE);
}
 
void handleStatus() {
    lastContactMs = millis();
    sendStatus();
}
 
// /set?on=1  /set?on=0  /set?speed=40  (can be combined)
void handleSet() {
    lastContactMs = millis();
    if (server.hasArg("speed")) {
        speedPercent = constrain(server.arg("speed").toInt(), 0, 100);
        Serial.printf(">>> Phone: speed %u%%\n", (unsigned)speedPercent);
    }
    if (server.hasArg("on")) {
        motorOn = server.arg("on") == "1";
        if (motorOn) lostConnection = false;
        Serial.printf(">>> Phone: %s\n", motorOn ? "Start" : "Stop");
    }
    updateMotor();  // act immediately
    sendStatus();
}
 
void handleNotFound() {
    server.send(404, "text/plain", "Not found");
}
 
// ------------------------------------------------------------
 
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("boot");
 
    // Motors first, held stopped
    motor1.begin();
    motor2.begin();
    stopAll();
    // Turn on the enable pins (GPIO 4 and 40). Does nothing harmful if the
    // EN pins are wired straight to 5V instead.
    motor1.enable();
    motor2.enable();
    Serial.println("Motors stopped, drivers enabled");
 
    // Wi-Fi hotspot
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(WIFI_NAME, WIFI_PASSWORD)) {
        halt(F("Wi-Fi hotspot failed to start (password must be 8+ characters)"));
    }
 
    server.on("/", handleRoot);
    server.on("/status", handleStatus);
    server.on("/set", handleSet);
    server.onNotFound(handleNotFound);
    server.begin();
 
    Serial.printf("Ready. Join \"%s\" (password %s) and open http://%s\n",
                  WIFI_NAME, WIFI_PASSWORD, WiFi.softAPIP().toString().c_str());
}
 
void loop() {
    server.handleClient();
 
    uint32_t now = millis();
 
    // Phone went quiet (locked, closed, out of range): switch the motor off
    if (motorOn && CONTROL_TIMEOUT_MS > 0 && now - lastContactMs > CONTROL_TIMEOUT_MS) {
        motorOn = false;
        lostConnection = true;
        Serial.println(">>> Lost contact with the phone - motor switched off");
    }
 
    updateMotor();
 
    // Status line once per second
    static uint32_t lastPrint = 0;
    if (now - lastPrint >= 1000) {
        lastPrint = now;
        Serial.printf("Start %s | speed %u%% | %s | phones connected %u\n",
                      motorOn ? "on" : "off", (unsigned)speedPercent,
                      appliedPwm ? "MOTOR ON" : "motor off",
                      (unsigned)WiFi.softAPgetStationNum());
    }
 
    delay(2);
}