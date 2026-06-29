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
  {"turtle", 100, 120, 180},
  {"normal", 240, 420, 520},
  {"full", 420, 1200, 1200},
};

struct DriveMix {
  bool link = false;
  bool armed = false;
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

HardwareSerial crsfSerial(1);
WebServer server(80);
DriveMix driveMix;

uint16_t channelsRaw[CRSF_CHANNEL_COUNT] = {};
uint16_t channelsUs[CRSF_CHANNEL_COUNT] = {};
uint32_t lastFrameMs = 0;
uint32_t frameCount = 0;
uint32_t crcFailCount = 0;
uint32_t badFrameCount = 0;

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
.control{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:12px;margin-bottom:14px;display:grid;grid-template-columns:repeat(7,minmax(0,1fr));gap:10px}
.metric{border:1px solid var(--line);border-radius:7px;padding:8px;background:#111512}.metric b{display:block;font-size:18px;font-variant-numeric:tabular-nums}.metric span{color:var(--muted);font-size:12px}
.ch{border:1px solid var(--line);background:var(--panel);border-radius:8px;padding:10px;display:grid;grid-template-columns:64px minmax(90px,160px) 1fr 72px;gap:10px;align-items:center;min-height:54px}
.idx{color:var(--muted);font-weight:700}.label{width:100%;border:1px solid var(--line);background:#111512;color:var(--text);border-radius:6px;padding:8px;font:inherit}
.bar{height:16px;border-radius:5px;background:#0a0c0b;border:1px solid #283029;overflow:hidden}.fill{height:100%;width:0%;background:linear-gradient(90deg,var(--ok),var(--hot));transition:width .08s linear}
.value{text-align:right;font-variant-numeric:tabular-nums;color:var(--muted)}.dead .fill{background:var(--bad)}.dead #link{color:var(--bad)}
@media (max-width:920px){.control{grid-template-columns:repeat(3,minmax(0,1fr))}}
@media (max-width:760px){.grid{grid-template-columns:1fr}.ch{grid-template-columns:50px 1fr 66px}.bar{grid-column:1/-1}.status{justify-content:flex-start}header{display:block}.control{grid-template-columns:repeat(2,minmax(0,1fr))}}
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
  <div class="metric"><b id="profile">turtle</b><span>profile</span></div>
  <div class="metric"><b id="accel">0</b><span>accel/s</span></div>
  <div class="metric"><b id="speed">0</b><span>speed cmd</span></div>
  <div class="metric"><b id="steer">0</b><span>steer cmd</span></div>
  <div class="metric"><b id="left">0</b><span>left motor</span></div>
  <div class="metric"><b id="right">0</b><span>right motor</span></div>
</section>
<section class="grid" id="channels"></section>
</main>
<script>
const root=document.getElementById('channels');
const names=["Roll","Pitch","Throttle","Yaw","Arm","Mode","Aux 3","Aux 4","Aux 5","Aux 6","Aux 7","Aux 8","Aux 9","Aux 10","Aux 11","Aux 12"];
for(let i=0;i<16;i++){
  const saved=localStorage.getItem('kosar.ch'+i)||names[i]||('CH '+(i+1));
  const row=document.createElement('div');
  row.className='ch';
  row.innerHTML=`<div class="idx">CH${i+1}</div><input class="label" value="${saved.replaceAll('"','&quot;')}"><div class="bar"><div class="fill"></div></div><div class="value">----</div>`;
  row.querySelector('input').addEventListener('input',e=>localStorage.setItem('kosar.ch'+i,e.target.value));
  root.appendChild(row);
}
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
    document.getElementById('profile').textContent=d.control.profile;
    document.getElementById('accel').textContent=d.control.accel;
    document.getElementById('speed').textContent=d.control.speed;
    document.getElementById('steer').textContent=d.control.steer;
    document.getElementById('left').textContent=d.control.left;
    document.getElementById('right').textContent=d.control.right;
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
  driveMix.armed = driveMix.link && channelsUs[CH_ARM] > 1500;
  driveMix.mode = modeFromUs(channelsUs[CH_MODE]);
  driveMix.profile = &DRIVE_PROFILES[driveMix.mode - 1];

  driveMix.targetSpeed = driveMix.armed ? rcToCommand(channelsUs[CH_PITCH], driveMix.profile->maxCommand) : 0;
  driveMix.targetSteer = driveMix.armed ? rcToCommand(channelsUs[CH_ROLL], driveMix.profile->maxCommand) : 0;

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

void handleApiChannels() {
  String json;
  json.reserve(1100);
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
  json += ",\"control\":{\"armed\":";
  json += driveMix.armed ? "true" : "false";
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

  crsfSerial.begin(CRSF_BAUD, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN);

  startWifi();

  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/channels", HTTP_GET, handleApiChannels);
  server.begin();

  if (MDNS.begin("kosar")) {
    Serial.println("mDNS: http://kosar.local/");
  }

  Serial.print("CRSF RX pin: GPIO");
  Serial.println(CRSF_RX_PIN);
}

void loop() {
  readCrsf();
  updateDriveMix();
  server.handleClient();
}
