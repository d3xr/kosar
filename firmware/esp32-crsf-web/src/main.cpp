#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#if __has_include("wifi_local.h")
#include "wifi_local.h"
#endif

static constexpr int CRSF_RX_PIN = 16;
static constexpr int CRSF_TX_PIN = 17;
static constexpr uint32_t CRSF_BAUD = 420000;
static constexpr int HOVER_RX_PIN = 26;
static constexpr int HOVER_TX_PIN = 25;
static constexpr uint32_t HOVER_BAUD = 115200;
static constexpr uint16_t HOVER_START_FRAME = 0xABCD;
static constexpr uint32_t HOVER_SEND_INTERVAL_MS = 20;
static constexpr int VBAT_ADC_PIN = 35;
static constexpr float VBAT_R_TOP = 390000.0f;
static constexpr float VBAT_R_BOTTOM = 27000.0f;
static constexpr uint32_t WEB_DRIVE_TIMEOUT_MS = 500;
static constexpr uint32_t MOTOR_TEST_TIMEOUT_MS = 500;
static constexpr int16_t MOTOR_TEST_MAX_COMMAND = 1000;

static constexpr const char *AP_SSID = "Kosar-RC";
static constexpr const char *AP_PASS = "kosar1234";

#ifndef KOSAR_WIFI_SSID
#define KOSAR_WIFI_SSID ""
#endif

#ifndef KOSAR_WIFI_PASS
#define KOSAR_WIFI_PASS ""
#endif

static constexpr uint8_t CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8;
static constexpr uint8_t CRSF_ADDRESS_CRSF_RECEIVER = 0xEC;
static constexpr uint8_t CRSF_ADDRESS_RADIO_TRANSMITTER = 0xEA;
static constexpr uint8_t CRSF_FRAMETYPE_RC_CHANNELS_PACKED = 0x16;
static constexpr size_t CRSF_MAX_FRAME_SIZE = 64;
static constexpr size_t CRSF_CHANNEL_PAYLOAD_SIZE = 22;
static constexpr size_t CRSF_CHANNEL_COUNT = 16;

static constexpr size_t CH_ROLL = 0;
static constexpr size_t CH_PITCH = 1;
static constexpr size_t CH_ARM = 4;
static constexpr size_t CH_MODE = 5;
static constexpr int RC_CENTER_US = 1500;
static constexpr int RC_DEADBAND_US = 35;

struct DriveProfile {
  const char *name;
  int16_t maxCommand;
  int16_t accelPerSec;
  int16_t steerPerSec;
};

static constexpr DriveProfile DRIVE_PROFILES[] = {
  {"low", 250, 250, 250},
  {"mid", 600, 800, 800},
  {"max", 1000, 2500, 2500},
};

static constexpr DriveProfile MOTOR_TEST_PROFILE = {"motor", MOTOR_TEST_MAX_COMMAND, 4000, 4000};

struct DriveMix {
  bool link = false;
  bool armed = false;
  bool webActive = false;
  bool motorTestActive = false;
  uint32_t ageMs = 999999;
  uint8_t mode = 1;
  const DriveProfile *profile = &DRIVE_PROFILES[0];
  int16_t targetSpeed = 0;
  int16_t targetSteer = 0;
  int16_t speed = 0;
  int16_t steer = 0;
  int16_t left = 0;
  int16_t right = 0;
  uint32_t lastUpdateMs = 0;
};

struct WebDriveState {
  bool enabled = false;
  bool armed = false;
  uint8_t mode = 1;
  int16_t speed = 0;
  int16_t steer = 0;
  uint32_t lastUpdateMs = 0;
};

struct MotorTestState {
  bool enabled = false;
  bool armed = false;
  bool reverse = false;
  int16_t left = 0;
  int16_t right = 0;
  uint32_t lastUpdateMs = 0;
};

struct __attribute__((packed)) HoverCommand {
  uint16_t start;
  int16_t steer;
  int16_t speed;
  uint16_t checksum;
};

struct __attribute__((packed)) HoverFeedback {
  uint16_t start;
  int16_t cmd1;
  int16_t cmd2;
  int16_t speedR;
  int16_t speedL;
  int16_t batVoltage;
  int16_t boardTemp;
  uint16_t cmdLed;
  uint16_t checksum;
};

HardwareSerial crsfSerial(1);
HardwareSerial hoverSerial(2);
WebServer server(80);
DriveMix driveMix;
WebDriveState webDrive;
MotorTestState motorTest;

uint16_t channelsRaw[CRSF_CHANNEL_COUNT] = {};
uint16_t channelsUs[CRSF_CHANNEL_COUNT] = {};
uint32_t lastFrameMs = 0;
uint32_t frameCount = 0;
uint32_t crcFailCount = 0;
uint32_t badFrameCount = 0;
float batteryVoltage = 0.0f;
uint32_t lastBatterySampleMs = 0;
uint32_t lastHoverSendMs = 0;
uint32_t hoverTxCount = 0;
uint32_t hoverRxCount = 0;
uint32_t hoverCrcFailCount = 0;
uint32_t lastHoverFeedbackMs = 0;
HoverFeedback hoverFeedback = {};
HoverFeedback hoverFeedbackIn = {};
uint8_t hoverFeedbackIndex = 0;
uint8_t *hoverFeedbackPtr = nullptr;
uint8_t hoverBytePrev = 0;

enum class ParserState {
  WaitAddress,
  ReadLength,
  ReadBody,
};

ParserState parserState = ParserState::WaitAddress;
uint8_t frameAddress = 0;
uint8_t frameLength = 0;
uint8_t frameBody[CRSF_MAX_FRAME_SIZE] = {};
uint8_t frameBodyIndex = 0;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kosar RC Bench</title>
<style>
:root{color-scheme:dark;--bg:#111312;--panel:#1b201d;--line:#303830;--text:#eff6ef;--muted:#9aa79b;--hot:#ffcc4d;--ok:#35d07f;--bad:#ff5a5f}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.35 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
main{width:min(1120px,100%);margin:0 auto;padding:18px}
header{display:flex;gap:12px;align-items:flex-end;justify-content:space-between;margin-bottom:14px}
h1{font-size:22px;margin:0;letter-spacing:0}p{margin:4px 0 0;color:var(--muted)}
.status{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
.pill{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:7px 9px;min-width:92px;text-align:center}
.pill b{display:block;font-size:17px}.pill span{color:var(--muted);font-size:12px}
.grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.control{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:12px;margin-bottom:14px;display:grid;grid-template-columns:repeat(8,minmax(0,1fr));gap:10px}
.feedback{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:12px;margin-bottom:14px;display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:10px}
.webdrive,.motortest{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:12px;margin-bottom:14px;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px;align-items:end}
.webdrive label,.motortest label{display:grid;gap:6px;color:var(--muted);font-size:12px}.webdrive input[type=range],.motortest input[type=range]{width:100%}.webdrive input[type=checkbox],.motortest input[type=checkbox]{width:22px;height:22px;accent-color:var(--hot)}
.webdrive select,.webdrive button,.motortest select,.motortest button{border:1px solid var(--line);background:#111512;color:var(--text);border-radius:6px;padding:9px;font:inherit}
.webdrive button,.motortest button{cursor:pointer}.webdrive button:active,.motortest button:active{transform:translateY(1px)}
.metric{border:1px solid var(--line);border-radius:7px;padding:8px;background:#111512}.metric b{display:block;font-size:18px;font-variant-numeric:tabular-nums}.metric span{color:var(--muted);font-size:12px}
.ch{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:10px;display:grid;grid-template-columns:64px minmax(90px,160px) 1fr 72px;gap:10px;align-items:center;min-height:54px}
.idx{color:var(--muted);font-weight:700}.label{width:100%;border:1px solid var(--line);background:#111512;color:var(--text);border-radius:6px;padding:8px;font:inherit}
.bar{height:16px;border-radius:5px;background:#0a0c0b;border:1px solid #283029;overflow:hidden}.fill{height:100%;width:0%;background:linear-gradient(90deg,var(--ok),var(--hot));transition:width .08s linear}
.value{text-align:right;font-variant-numeric:tabular-nums;color:var(--muted)}.dead .fill{background:var(--bad)}.dead #link{color:var(--bad)}
@media (max-width:920px){.control{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media (max-width:920px){.feedback{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media (max-width:920px){.webdrive,.motortest{grid-template-columns:repeat(2,minmax(0,1fr))}}
@media (max-width:760px){.grid{grid-template-columns:1fr}.ch{grid-template-columns:50px 1fr 66px}.bar{grid-column:1/-1}.status{justify-content:flex-start}header{display:block}.control,.feedback,.webdrive,.motortest{grid-template-columns:repeat(2,minmax(0,1fr))}}
</style>
</head>
<body>
<main>
<header>
  <div>
    <h1>Kosar RC Bench</h1>
    <p>ELRS/CRSF live channels. Labels stay only in this browser.</p>
  </div>
  <div class="status">
    <div class="pill"><b id="link">...</b><span>link</span></div>
    <div class="pill"><b id="age">...</b><span>age ms</span></div>
    <div class="pill"><b id="frames">0</b><span>frames</span></div>
    <div class="pill"><b id="crc">0</b><span>crc fail</span></div>
  </div>
</header>
<section class="control">
  <div class="metric"><b id="armed">NO</b><span>armed</span></div>
  <div class="metric"><b id="profile">low</b><span>profile</span></div>
  <div class="metric"><b id="accel">0</b><span>accel/s</span></div>
  <div class="metric"><b id="speed">0</b><span>speed cmd</span></div>
  <div class="metric"><b id="steer">0</b><span>steer cmd</span></div>
  <div class="metric"><b id="left">0</b><span>left motor</span></div>
  <div class="metric"><b id="right">0</b><span>right motor</span></div>
  <div class="metric"><b id="battery">--.-</b><span>battery V</span></div>
</section>
<section class="feedback">
  <div class="metric"><b id="hover">NO</b><span>hoverboard</span></div>
  <div class="metric"><b id="hcmd">0 / 0</b><span>cmd steer/speed</span></div>
  <div class="metric"><b id="hspeed">0 / 0</b><span>rpm R/L</span></div>
  <div class="metric"><b id="hbat">0</b><span>board bat</span></div>
  <div class="metric"><b id="htemp">0</b><span>board temp</span></div>
  <div class="metric"><b id="hcrc">0</b><span>hover crc fail</span></div>
</section>
<section class="webdrive">
  <label>web drive<input id="webEnabled" type="checkbox"></label>
  <label>web arm<input id="webArm" type="checkbox"></label>
  <label>profile<select id="webMode"><option value="1">low</option><option value="2">mid</option><option value="3">max</option></select></label>
  <button id="webStop" type="button">STOP</button>
  <label>speed <b id="webSpeedVal">0</b><input id="webSpeed" type="range" min="-100" max="100" value="0"></label>
  <label>steer <b id="webSteerVal">0</b><input id="webSteer" type="range" min="-100" max="100" value="0"></label>
  <div class="metric"><b id="source">rc</b><span>source</span></div>
  <div class="metric"><b id="webAge">---</b><span>web age ms</span></div>
</section>
<section class="motortest">
  <label>motor test<input id="motorEnabled" type="checkbox"></label>
  <label>test arm<input id="motorArm" type="checkbox"></label>
  <label>direction<select id="motorDirection"><option value="0">forward</option><option value="1">reverse</option></select></label>
  <button id="motorStop" type="button">ZERO</button>
  <label>left <b id="motorLeftVal">0</b><input id="motorLeft" type="range" min="0" max="100" value="0"></label>
  <label>right <b id="motorRightVal">0</b><input id="motorRight" type="range" min="0" max="100" value="0"></label>
  <label>both <b id="motorBothVal">0</b><input id="motorBoth" type="range" min="0" max="100" value="0"></label>
  <div class="metric"><b id="motorAge">---</b><span>motor age ms</span></div>
</section>
<section class="grid" id="channels"></section>
</main>
<script>
const root=document.getElementById('channels');
const webEnabled=document.getElementById('webEnabled');
const webArm=document.getElementById('webArm');
const webMode=document.getElementById('webMode');
const webSpeed=document.getElementById('webSpeed');
const webSteer=document.getElementById('webSteer');
const webSpeedVal=document.getElementById('webSpeedVal');
const webSteerVal=document.getElementById('webSteerVal');
const motorEnabled=document.getElementById('motorEnabled');
const motorArm=document.getElementById('motorArm');
const motorDirection=document.getElementById('motorDirection');
const motorLeft=document.getElementById('motorLeft');
const motorRight=document.getElementById('motorRight');
const motorBoth=document.getElementById('motorBoth');
const motorLeftVal=document.getElementById('motorLeftVal');
const motorRightVal=document.getElementById('motorRightVal');
const motorBothVal=document.getElementById('motorBothVal');
let lastWebSend=0;
let lastMotorSend=0;
const names=["Roll","Pitch","Throttle","Yaw","Arm","Mode","Aux 3","Aux 4","Aux 5","Aux 6","Aux 7","Aux 8","Aux 9","Aux 10","Aux 11","Aux 12"];
for(let i=0;i<16;i++){
  const saved=localStorage.getItem('kosar.ch'+i)||names[i]||('CH '+(i+1));
  const row=document.createElement('div');
  row.className='ch';
  row.innerHTML=`<div class="idx">CH${i+1}</div><input class="label" value="${saved.replaceAll('"','&quot;')}"><div class="bar"><div class="fill"></div></div><div class="value">----</div>`;
  row.querySelector('input').addEventListener('input',e=>localStorage.setItem('kosar.ch'+i,e.target.value));
  root.appendChild(row);
}
function stopWebDrive(){
  webEnabled.checked=false;
  webArm.checked=false;
  webSpeed.value=0;
  webSteer.value=0;
  webSpeedVal.textContent='0';
  webSteerVal.textContent='0';
  sendWebDrive(true);
}
function disableWebDrive(){
  webEnabled.checked=false;
  webArm.checked=false;
  webSpeed.value=0;
  webSteer.value=0;
  webSpeedVal.textContent='0';
  webSteerVal.textContent='0';
  sendWebDrive(true);
}
async function sendWebDrive(force=false){
  const now=Date.now();
  if(!force && now-lastWebSend<80)return;
  lastWebSend=now;
  webSpeedVal.textContent=webSpeed.value;
  webSteerVal.textContent=webSteer.value;
  const params=new URLSearchParams({
    enabled:webEnabled.checked?'1':'0',
    armed:webArm.checked?'1':'0',
    mode:webMode.value,
    speed:webSpeed.value,
    steer:webSteer.value
  });
  try{await fetch('/api/webdrive?'+params.toString(),{cache:'no-store'});}catch(e){}
}
function zeroMotorTest(){
  motorArm.checked=false;
  motorLeft.value=0;
  motorRight.value=0;
  motorBoth.value=0;
  motorLeftVal.textContent='0';
  motorRightVal.textContent='0';
  motorBothVal.textContent='0';
  sendMotorTest(true);
}
function disableMotorTest(){
  motorEnabled.checked=false;
  motorArm.checked=false;
  motorLeft.value=0;
  motorRight.value=0;
  motorBoth.value=0;
  motorLeftVal.textContent='0';
  motorRightVal.textContent='0';
  motorBothVal.textContent='0';
  sendMotorTest(true);
}
async function sendMotorTest(force=false){
  const now=Date.now();
  if(!force && now-lastMotorSend<80)return;
  lastMotorSend=now;
  motorLeftVal.textContent=motorLeft.value;
  motorRightVal.textContent=motorRight.value;
  motorBothVal.textContent=motorBoth.value;
  const params=new URLSearchParams({
    enabled:motorEnabled.checked?'1':'0',
    armed:motorArm.checked?'1':'0',
    reverse:motorDirection.value,
    left:motorLeft.value,
    right:motorRight.value
  });
  try{await fetch('/api/motortest?'+params.toString(),{cache:'no-store'});}catch(e){}
}
document.getElementById('webStop').addEventListener('click',stopWebDrive);
document.getElementById('motorStop').addEventListener('click',zeroMotorTest);
webEnabled.addEventListener('input',()=>{if(webEnabled.checked)disableMotorTest();sendWebDrive(true)});
[webArm,webMode,webSpeed,webSteer].forEach(el=>el.addEventListener('input',()=>sendWebDrive(true)));
motorBoth.addEventListener('input',()=>{
  motorLeft.value=motorBoth.value;
  motorRight.value=motorBoth.value;
  sendMotorTest(true);
});
motorEnabled.addEventListener('input',()=>{if(motorEnabled.checked)disableWebDrive();sendMotorTest(true)});
[motorArm,motorDirection,motorLeft,motorRight].forEach(el=>el.addEventListener('input',()=>sendMotorTest(true)));
setInterval(()=>{if(webEnabled.checked)sendWebDrive(false)},100);
setInterval(()=>{if(motorEnabled.checked)sendMotorTest(false)},100);
async function tick(){
  try{
    const r=await fetch('/api/channels',{cache:'no-store'});
    const d=await r.json();
    document.body.classList.toggle('dead',!d.link);
    document.getElementById('link').textContent=d.link?'OK':'NO';
    document.getElementById('age').textContent=d.age_ms;
    document.getElementById('frames').textContent=d.frames;
    document.getElementById('crc').textContent=d.crc_fail;
    document.getElementById('armed').textContent=d.control.armed?'YES':'NO';
    document.getElementById('source').textContent=d.control.source;
    document.getElementById('webAge').textContent=d.web_drive.age_ms;
    document.getElementById('motorAge').textContent=d.motor_test.age_ms;
    document.getElementById('profile').textContent=d.control.profile;
    document.getElementById('accel').textContent=d.control.accel;
    document.getElementById('speed').textContent=d.control.speed;
    document.getElementById('steer').textContent=d.control.steer;
    document.getElementById('left').textContent=d.control.left;
    document.getElementById('right').textContent=d.control.right;
    document.getElementById('battery').textContent=d.battery_v.toFixed(1);
    document.getElementById('hover').textContent=d.hover.link?'OK':'NO';
    document.getElementById('hcmd').textContent=d.hover.cmd1+' / '+d.hover.cmd2;
    document.getElementById('hspeed').textContent=d.hover.speed_r+' / '+d.hover.speed_l;
    document.getElementById('hbat').textContent=d.hover.bat;
    document.getElementById('htemp').textContent=d.hover.temp;
    document.getElementById('hcrc').textContent=d.hover.crc_fail;
    d.channels.forEach((ch,i)=>{
      const row=root.children[i];
      const pct=Math.max(0,Math.min(100,(ch.us-988)/(2012-988)*100));
      row.querySelector('.fill').style.width=pct+'%';
      row.querySelector('.value').textContent=ch.us+' us';
    });
  }catch(e){
    document.body.classList.add('dead');
    document.getElementById('link').textContent='NO';
  }
}
setInterval(tick,100);tick();
</script>
</body>
</html>
)HTML";

uint8_t crc8DvbS2(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  while (len--) {
    crc ^= *data++;
    for (uint8_t i = 0; i < 8; i++) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0xD5) : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool isLikelyAddress(uint8_t value) {
  return value == CRSF_ADDRESS_FLIGHT_CONTROLLER ||
         value == CRSF_ADDRESS_CRSF_RECEIVER ||
         value == CRSF_ADDRESS_RADIO_TRANSMITTER;
}

uint16_t crsfToMicroseconds(uint16_t raw) {
  int32_t us = ((static_cast<int32_t>(raw) - 992) * 5 / 8) + 1500;
  return static_cast<uint16_t>(constrain(us, 880, 2120));
}

int16_t rcToCommand(uint16_t us, int16_t limit) {
  const int delta = static_cast<int>(us) - RC_CENTER_US;
  if (abs(delta) <= RC_DEADBAND_US) {
    return 0;
  }

  const int input = delta > 0 ? delta - RC_DEADBAND_US : delta + RC_DEADBAND_US;
  const int inputMax = 500 - RC_DEADBAND_US;
  return static_cast<int16_t>(constrain((input * limit) / inputMax, -limit, limit));
}

uint8_t modeFromUs(uint16_t modeUs) {
  if (modeUs < 1300) {
    return 1;
  }
  if (modeUs < 1700) {
    return 2;
  }
  return 3;
}

uint8_t clampMode(int value) {
  if (value < 1) {
    return 1;
  }
  if (value > 3) {
    return 3;
  }
  return static_cast<uint8_t>(value);
}

int16_t webPercentToCommand(int value, int16_t limit) {
  const int percent = constrain(value, -100, 100);
  return static_cast<int16_t>((percent * limit) / 100);
}

int16_t motorPercentToCommand(int value, bool reverse) {
  const int percent = constrain(value, 0, 100);
  const int sign = reverse ? -1 : 1;
  return static_cast<int16_t>((percent * MOTOR_TEST_MAX_COMMAND * sign) / 100);
}

int16_t stepToward(int16_t current, int16_t target, int16_t ratePerSec, uint32_t dtMs) {
  const int32_t delta = static_cast<int32_t>(target) - current;
  const int32_t maxStep = max<int32_t>(1, (static_cast<int32_t>(ratePerSec) * dtMs) / 1000);
  if (delta > maxStep) {
    return current + maxStep;
  }
  if (delta < -maxStep) {
    return current - maxStep;
  }
  return target;
}

void updateDriveMix() {
  const uint32_t now = millis();
  const uint32_t dtMs = driveMix.lastUpdateMs == 0 ? 0 : now - driveMix.lastUpdateMs;
  driveMix.lastUpdateMs = now;

  driveMix.ageMs = lastFrameMs == 0 ? 999999 : now - lastFrameMs;
  driveMix.link = lastFrameMs != 0 && driveMix.ageMs < 500;
  driveMix.webActive = webDrive.enabled && webDrive.lastUpdateMs != 0 &&
                       now - webDrive.lastUpdateMs < WEB_DRIVE_TIMEOUT_MS;
  driveMix.motorTestActive = motorTest.enabled && motorTest.lastUpdateMs != 0 &&
                             now - motorTest.lastUpdateMs < MOTOR_TEST_TIMEOUT_MS;

  if (driveMix.motorTestActive) {
    driveMix.armed = motorTest.armed;
    driveMix.mode = 3;
  } else if (driveMix.webActive) {
    driveMix.armed = webDrive.armed;
    driveMix.mode = webDrive.mode;
  } else {
    driveMix.armed = driveMix.link && channelsUs[CH_ARM] > 1500;
    driveMix.mode = modeFromUs(channelsUs[CH_MODE]);
  }

  driveMix.profile = driveMix.motorTestActive ? &MOTOR_TEST_PROFILE : &DRIVE_PROFILES[driveMix.mode - 1];

  if (!driveMix.armed) {
    driveMix.targetSpeed = 0;
    driveMix.targetSteer = 0;
  } else if (driveMix.motorTestActive) {
    const int16_t leftTarget = motorPercentToCommand(motorTest.left, motorTest.reverse);
    const int16_t rightTarget = motorPercentToCommand(motorTest.right, motorTest.reverse);
    driveMix.targetSpeed = static_cast<int16_t>((static_cast<int32_t>(leftTarget) + rightTarget) / 2);
    driveMix.targetSteer = static_cast<int16_t>((static_cast<int32_t>(leftTarget) - rightTarget) / 2);
  } else if (driveMix.webActive) {
    driveMix.targetSpeed = webPercentToCommand(webDrive.speed, driveMix.profile->maxCommand);
    driveMix.targetSteer = webPercentToCommand(webDrive.steer, driveMix.profile->maxCommand);
  } else {
    driveMix.targetSpeed = rcToCommand(channelsUs[CH_PITCH], driveMix.profile->maxCommand);
    driveMix.targetSteer = rcToCommand(channelsUs[CH_ROLL], driveMix.profile->maxCommand);
  }

  if (!driveMix.armed) {
    driveMix.speed = 0;
    driveMix.steer = 0;
  } else {
    driveMix.speed = stepToward(driveMix.speed, driveMix.targetSpeed, driveMix.profile->accelPerSec, dtMs);
    driveMix.steer = stepToward(driveMix.steer, driveMix.targetSteer, driveMix.profile->steerPerSec, dtMs);
  }

  driveMix.left = constrain(
    driveMix.speed + driveMix.steer,
    -static_cast<int>(driveMix.profile->maxCommand),
    static_cast<int>(driveMix.profile->maxCommand)
  );
  driveMix.right = constrain(
    driveMix.speed - driveMix.steer,
    -static_cast<int>(driveMix.profile->maxCommand),
    static_cast<int>(driveMix.profile->maxCommand)
  );
}

void decodeChannels(const uint8_t *payload, size_t len) {
  if (len < CRSF_CHANNEL_PAYLOAD_SIZE) {
    badFrameCount++;
    return;
  }

  uint32_t bitBuffer = 0;
  uint8_t bitsInBuffer = 0;
  size_t byteIndex = 0;

  for (size_t channel = 0; channel < CRSF_CHANNEL_COUNT; channel++) {
    while (bitsInBuffer < 11 && byteIndex < len) {
      bitBuffer |= static_cast<uint32_t>(payload[byteIndex++]) << bitsInBuffer;
      bitsInBuffer += 8;
    }

    channelsRaw[channel] = bitBuffer & 0x07FF;
    channelsUs[channel] = crsfToMicroseconds(channelsRaw[channel]);
    bitBuffer >>= 11;
    bitsInBuffer -= 11;
  }

  lastFrameMs = millis();
  frameCount++;
}

void handleFrame() {
  if (frameLength < 2) {
    badFrameCount++;
    return;
  }

  const uint8_t expectedCrc = frameBody[frameLength - 1];
  const uint8_t actualCrc = crc8DvbS2(frameBody, frameLength - 1);
  if (actualCrc != expectedCrc) {
    crcFailCount++;
    return;
  }

  const uint8_t frameType = frameBody[0];
  const uint8_t *payload = frameBody + 1;
  const size_t payloadLen = frameLength - 2;

  if (frameType == CRSF_FRAMETYPE_RC_CHANNELS_PACKED) {
    decodeChannels(payload, payloadLen);
  }
}

void readCrsf() {
  while (crsfSerial.available()) {
    const uint8_t value = crsfSerial.read();

    switch (parserState) {
      case ParserState::WaitAddress:
        if (isLikelyAddress(value)) {
          frameAddress = value;
          parserState = ParserState::ReadLength;
        }
        break;

      case ParserState::ReadLength:
        frameLength = value;
        frameBodyIndex = 0;
        if (frameLength < 2 || frameLength > CRSF_MAX_FRAME_SIZE) {
          parserState = ParserState::WaitAddress;
          badFrameCount++;
        } else {
          parserState = ParserState::ReadBody;
        }
        break;

      case ParserState::ReadBody:
        frameBody[frameBodyIndex++] = value;
        if (frameBodyIndex >= frameLength) {
          (void)frameAddress;
          handleFrame();
          parserState = ParserState::WaitAddress;
        }
        break;
    }
  }
}

void handleIndex() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleWebDrive() {
  if (server.hasArg("enabled")) {
    webDrive.enabled = server.arg("enabled") == "1";
  }
  if (server.hasArg("armed")) {
    webDrive.armed = server.arg("armed") == "1";
  }
  if (server.hasArg("mode")) {
    webDrive.mode = clampMode(server.arg("mode").toInt());
  }
  if (server.hasArg("speed")) {
    webDrive.speed = constrain(server.arg("speed").toInt(), -100, 100);
  }
  if (server.hasArg("steer")) {
    webDrive.steer = constrain(server.arg("steer").toInt(), -100, 100);
  }

  if (!webDrive.enabled) {
    webDrive.armed = false;
    webDrive.speed = 0;
    webDrive.steer = 0;
  } else {
    motorTest.enabled = false;
    motorTest.armed = false;
    motorTest.left = 0;
    motorTest.right = 0;
  }

  webDrive.lastUpdateMs = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMotorTest() {
  if (server.hasArg("enabled")) {
    motorTest.enabled = server.arg("enabled") == "1";
  }
  if (server.hasArg("armed")) {
    motorTest.armed = server.arg("armed") == "1";
  }
  if (server.hasArg("reverse")) {
    motorTest.reverse = server.arg("reverse") == "1";
  }
  if (server.hasArg("left")) {
    motorTest.left = constrain(server.arg("left").toInt(), 0, 100);
  }
  if (server.hasArg("right")) {
    motorTest.right = constrain(server.arg("right").toInt(), 0, 100);
  }

  if (!motorTest.enabled) {
    motorTest.armed = false;
    motorTest.left = 0;
    motorTest.right = 0;
  } else {
    webDrive.enabled = false;
    webDrive.armed = false;
    webDrive.speed = 0;
    webDrive.steer = 0;
  }

  motorTest.lastUpdateMs = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}

void sendHoverCommand() {
  const uint32_t now = millis();
  if (now - lastHoverSendMs < HOVER_SEND_INTERVAL_MS) {
    return;
  }
  lastHoverSendMs = now;

  HoverCommand command = {};
  command.start = HOVER_START_FRAME;
  command.steer = driveMix.steer;
  command.speed = driveMix.speed;
  command.checksum = command.start ^ command.steer ^ command.speed;

  hoverSerial.write(reinterpret_cast<uint8_t *>(&command), sizeof(command));
  hoverTxCount++;
}

bool hoverFeedbackChecksumOk(const HoverFeedback &feedback) {
  const uint16_t checksum = feedback.start ^ feedback.cmd1 ^ feedback.cmd2 ^
                            feedback.speedR ^ feedback.speedL ^ feedback.batVoltage ^
                            feedback.boardTemp ^ feedback.cmdLed;
  return feedback.start == HOVER_START_FRAME && checksum == feedback.checksum;
}

void readHoverFeedback() {
  while (hoverSerial.available()) {
    const uint8_t incomingByte = hoverSerial.read();
    const uint16_t startFrame = (static_cast<uint16_t>(incomingByte) << 8) | hoverBytePrev;

    if (startFrame == HOVER_START_FRAME) {
      hoverFeedbackPtr = reinterpret_cast<uint8_t *>(&hoverFeedbackIn);
      *hoverFeedbackPtr++ = hoverBytePrev;
      *hoverFeedbackPtr++ = incomingByte;
      hoverFeedbackIndex = 2;
    } else if (hoverFeedbackIndex >= 2 && hoverFeedbackIndex < sizeof(HoverFeedback)) {
      *hoverFeedbackPtr++ = incomingByte;
      hoverFeedbackIndex++;
    }

    if (hoverFeedbackIndex == sizeof(HoverFeedback)) {
      if (hoverFeedbackChecksumOk(hoverFeedbackIn)) {
        hoverFeedback = hoverFeedbackIn;
        hoverRxCount++;
        lastHoverFeedbackMs = millis();
      } else {
        hoverCrcFailCount++;
      }
      hoverFeedbackIndex = 0;
    }

    hoverBytePrev = incomingByte;
  }
}

void updateBatteryVoltage() {
  const uint32_t now = millis();
  if (now - lastBatterySampleMs < 200) {
    return;
  }
  lastBatterySampleMs = now;

  uint32_t mvSum = 0;
  for (uint8_t i = 0; i < 8; i++) {
    mvSum += analogReadMilliVolts(VBAT_ADC_PIN);
  }

  const float adcVoltage = (mvSum / 8.0f) / 1000.0f;
  const float divider = (VBAT_R_TOP + VBAT_R_BOTTOM) / VBAT_R_BOTTOM;
  batteryVoltage = adcVoltage * divider;
}

void handleApiChannels() {
  const uint32_t hoverAgeMs = lastHoverFeedbackMs == 0 ? 999999 : millis() - lastHoverFeedbackMs;
  const bool hoverLink = lastHoverFeedbackMs != 0 && hoverAgeMs < 500;
  const uint32_t webAgeMs = webDrive.lastUpdateMs == 0 ? 999999 : millis() - webDrive.lastUpdateMs;
  const uint32_t motorAgeMs = motorTest.lastUpdateMs == 0 ? 999999 : millis() - motorTest.lastUpdateMs;

  String json;
  json.reserve(2100);
  json += "{\"link\":";
  json += driveMix.link ? "true" : "false";
  json += ",\"age_ms\":";
  json += driveMix.ageMs;
  json += ",\"frames\":";
  json += frameCount;
  json += ",\"crc_fail\":";
  json += crcFailCount;
  json += ",\"bad_frames\":";
  json += badFrameCount;
  json += ",\"battery_v\":";
  json += String(batteryVoltage, 2);
  json += ",\"web_drive\":{\"enabled\":";
  json += webDrive.enabled ? "true" : "false";
  json += ",\"armed\":";
  json += webDrive.armed ? "true" : "false";
  json += ",\"active\":";
  json += driveMix.webActive ? "true" : "false";
  json += ",\"age_ms\":";
  json += webAgeMs;
  json += ",\"speed\":";
  json += webDrive.speed;
  json += ",\"steer\":";
  json += webDrive.steer;
  json += ",\"mode\":";
  json += webDrive.mode;
  json += '}';
  json += ",\"motor_test\":{\"enabled\":";
  json += motorTest.enabled ? "true" : "false";
  json += ",\"armed\":";
  json += motorTest.armed ? "true" : "false";
  json += ",\"active\":";
  json += driveMix.motorTestActive ? "true" : "false";
  json += ",\"reverse\":";
  json += motorTest.reverse ? "true" : "false";
  json += ",\"age_ms\":";
  json += motorAgeMs;
  json += ",\"left\":";
  json += motorTest.left;
  json += ",\"right\":";
  json += motorTest.right;
  json += '}';
  json += ",\"hover\":{\"link\":";
  json += hoverLink ? "true" : "false";
  json += ",\"age_ms\":";
  json += hoverAgeMs;
  json += ",\"tx\":";
  json += hoverTxCount;
  json += ",\"rx\":";
  json += hoverRxCount;
  json += ",\"crc_fail\":";
  json += hoverCrcFailCount;
  json += ",\"cmd1\":";
  json += hoverFeedback.cmd1;
  json += ",\"cmd2\":";
  json += hoverFeedback.cmd2;
  json += ",\"speed_r\":";
  json += hoverFeedback.speedR;
  json += ",\"speed_l\":";
  json += hoverFeedback.speedL;
  json += ",\"bat\":";
  json += hoverFeedback.batVoltage;
  json += ",\"temp\":";
  json += hoverFeedback.boardTemp;
  json += '}';
  json += ",\"control\":{\"armed\":";
  json += driveMix.armed ? "true" : "false";
  json += ",\"source\":\"";
  json += driveMix.motorTestActive ? "motor" : (driveMix.webActive ? "web" : "rc");
  json += "\"";
  json += ",\"mode\":";
  json += driveMix.mode;
  json += ",\"profile\":\"";
  json += driveMix.profile->name;
  json += "\",\"max\":";
  json += driveMix.profile->maxCommand;
  json += ",\"accel\":";
  json += driveMix.profile->accelPerSec;
  json += ",\"target_speed\":";
  json += driveMix.targetSpeed;
  json += ",\"target_steer\":";
  json += driveMix.targetSteer;
  json += ",\"speed\":";
  json += driveMix.speed;
  json += ",\"steer\":";
  json += driveMix.steer;
  json += ",\"left\":";
  json += driveMix.left;
  json += ",\"right\":";
  json += driveMix.right;
  json += '}';
  json += ",\"channels\":[";
  for (size_t i = 0; i < CRSF_CHANNEL_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += "{\"raw\":";
    json += channelsRaw[i];
    json += ",\"us\":";
    json += channelsUs[i];
    json += '}';
  }
  json += "]}";

  server.send(200, "application/json", json);
}

void startWifi() {
  if (strlen(KOSAR_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(KOSAR_WIFI_SSID, KOSAR_WIFI_PASS);

    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(KOSAR_WIFI_SSID);

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 10000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("STA IP: ");
      Serial.println(WiFi.localIP());
      return;
    }

    Serial.println("Home Wi-Fi failed, falling back to AP");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  Serial.print("AP: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("Kosar RC Bench boot");

  analogReadResolution(12);
  analogSetPinAttenuation(VBAT_ADC_PIN, ADC_11db);

  crsfSerial.begin(CRSF_BAUD, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN);
  hoverSerial.begin(HOVER_BAUD, SERIAL_8N1, HOVER_RX_PIN, HOVER_TX_PIN);

  startWifi();

  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/channels", HTTP_GET, handleApiChannels);
  server.on("/api/webdrive", HTTP_GET, handleWebDrive);
  server.on("/api/motortest", HTTP_GET, handleMotorTest);
  server.begin();

  if (MDNS.begin("kosar")) {
    Serial.println("mDNS: http://kosar.local/");
  }

  Serial.print("CRSF RX pin: GPIO");
  Serial.println(CRSF_RX_PIN);
  Serial.print("Hover RX/TX pins: GPIO");
  Serial.print(HOVER_RX_PIN);
  Serial.print("/GPIO");
  Serial.println(HOVER_TX_PIN);
}

void loop() {
  readCrsf();
  updateDriveMix();
  sendHoverCommand();
  readHoverFeedback();
  updateBatteryVoltage();
  server.handleClient();
}
