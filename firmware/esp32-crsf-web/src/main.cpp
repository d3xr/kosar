#include <Arduino.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_system.h>

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
static constexpr int16_t HOVER_COAST_COMMAND = INT16_MIN;
static constexpr int VBAT_ADC_PIN = 35;
static constexpr float VBAT_R_TOP = 390000.0f;
static constexpr float VBAT_R_BOTTOM = 27000.0f;
static constexpr uint32_t WEB_DRIVE_TIMEOUT_MS = 500;
static constexpr uint32_t MOTOR_TEST_TIMEOUT_MS = 500;
static constexpr int16_t MOTOR_TEST_MAX_COMMAND = 1000;
static constexpr size_t LOG_EVENT_CAPACITY = 128;
static constexpr uint32_t COAST_HOLD_MS = 250;
static constexpr uint32_t COMMAND_LOG_MIN_INTERVAL_MS = 500;

static constexpr const char *AP_SSID = "Kosar-RC";
static constexpr const char *AP_PASS = "kosar1234";
static constexpr const char *HOSTNAME = "kosar";
static constexpr const char *CONTROL_TOKEN_HEADER = "X-Control-Token";
static const char *CONTROL_HEADER_KEYS[] = {CONTROL_TOKEN_HEADER};

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

struct LogEvent {
  uint32_t seq = 0;
  uint32_t ms = 0;
  char level[6] = {};
  char type[12] = {};
  char msg[48] = {};
  int32_t a = 0;
  int32_t b = 0;
};

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
LogEvent logEvents[LOG_EVENT_CAPACITY] = {};
uint32_t logSeqLatest = 0;
uint32_t logDropped = 0;
size_t logCount = 0;
size_t logWriteIndex = 0;
bool wifiApMode = false;
bool mdnsStarted = false;
uint32_t forceCoastUntilMs = 0;
char controlToken[17] = {};
uint32_t lastWebCommandLogMs = 0;
uint32_t lastMotorCommandLogMs = 0;

bool prevCrsfLink = false;
bool prevHoverLink = false;
bool prevArmed = false;
bool prevWebActive = false;
bool prevMotorActive = false;

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
<link rel="icon" href="data:,">
<title>Kosar RC Bench</title>
<style>
:root{color-scheme:dark;--bg:#090b0a;--panel:#151914;--panel2:#0d100d;--line:#354036;--text:#f3f7ef;--muted:#94a195;--hot:#ffd84d;--acid:#63ff7a;--wire:#6ee7ff;--pink:#ff4fd8;--bad:#ff425d;--warn:#ff9b3d}
*{box-sizing:border-box}body{margin:0;background-color:var(--bg);background-image:linear-gradient(90deg,rgba(255,216,77,.055) 1px,transparent 1px),linear-gradient(0deg,rgba(110,231,255,.04) 1px,transparent 1px),repeating-linear-gradient(135deg,rgba(255,255,255,.025) 0 1px,transparent 1px 14px);background-size:44px 44px,44px 44px,auto;color:var(--text);font:15px/1.35 system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}
main{width:min(1180px,100%);margin:0 auto;padding:16px;border-top:3px solid var(--hot)}header{display:flex;align-items:flex-end;justify-content:space-between;gap:14px;margin-bottom:12px}
h1{margin:0;font-size:24px;letter-spacing:0;font-weight:900;text-transform:uppercase}h1:before{content:"// ";color:var(--hot)}.sub{margin:3px 0 0;color:var(--muted);font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}.top{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end}
.pill,.metric{position:relative;border:1px solid var(--line);border-left:3px solid var(--acid);background:linear-gradient(180deg,rgba(21,25,20,.96),rgba(13,16,13,.96));border-radius:5px;padding:8px 10px;box-shadow:0 10px 28px rgba(0,0,0,.18)}.pill{min-width:90px;text-align:center}.pill:nth-child(3){border-left-color:var(--bad)}.pill:nth-child(4){border-left-color:var(--wire)}.pill b,.metric b{display:block;font:800 18px/1.05 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-variant-numeric:tabular-nums}.pill span,.metric span,label span{display:block;color:var(--muted);font-size:11px;text-transform:uppercase}
.tabs{display:flex;gap:7px;overflow:auto;margin:10px 0 14px;padding-bottom:2px}.tab{border:1px solid var(--line);border-bottom:2px solid #111811;background:#0d120e;color:var(--muted);border-radius:5px;padding:9px 12px;font:800 13px/1.1 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;white-space:nowrap;cursor:pointer;text-transform:uppercase}.tab.active{color:var(--text);border-color:#6d7444;background:#171b15;box-shadow:inset 0 -3px 0 var(--hot)}
.panel{display:none}.panel.active{display:block}.grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:10px}.grid6{grid-template-columns:repeat(6,minmax(0,1fr))}.controls{border:1px solid var(--line);border-top:3px solid #232e25;background:rgba(15,19,15,.95);border-radius:5px;padding:12px;margin-top:12px;display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px;align-items:end}
label{display:grid;gap:7px;color:var(--muted);font-size:12px}input,select,button{font:inherit}input[type=range]{width:100%;accent-color:var(--hot)}input[type=checkbox]{width:24px;height:24px;accent-color:var(--hot)}
select,.button{border:1px solid var(--line);background:#090c09;color:var(--text);border-radius:5px;padding:10px}.button{cursor:pointer;font-weight:900;text-transform:uppercase}.button:active{transform:translateY(1px)}.danger{border-color:#79313b;background:#260f14;color:#ffd7dc;box-shadow:inset 0 -3px 0 var(--bad)}.zero{border-color:#766420;background:#211d0d;color:#fff0ad;box-shadow:inset 0 -3px 0 var(--hot)}
.channels{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:9px}.ch{border:1px solid var(--line);border-left:3px solid #222d25;background:rgba(15,19,15,.95);border-radius:5px;padding:10px;display:grid;grid-template-columns:54px minmax(95px,160px) 1fr 72px;gap:10px;align-items:center;min-height:54px}.idx{color:var(--hot);font:900 13px/1 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}.label{width:100%;border:1px solid var(--line);background:#090c09;color:var(--text);border-radius:5px;padding:8px}.bar{height:16px;border-radius:3px;background:#050805;border:1px solid #293129;overflow:hidden}.fill{height:100%;width:0;background:linear-gradient(90deg,var(--acid),var(--hot),var(--pink));transition:width .08s linear}.value{text-align:right;font:700 13px/1 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;color:var(--muted)}
.logbar{display:flex;gap:10px;align-items:center;justify-content:space-between;margin-bottom:10px}.logs{height:52vh;min-height:320px;overflow:auto;border:1px solid var(--line);background:#050805;border-radius:5px;padding:8px;font:13px/1.35 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}.log{display:grid;grid-template-columns:70px 56px 92px 1fr 52px 52px;gap:8px;padding:5px 4px;border-bottom:1px solid #141a15}.log.warn{color:#ffd18a}.log.error{color:#ff9aa4}.chip{border:1px solid var(--line);border-radius:4px;padding:2px 7px;color:var(--muted);font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
.dead #link,.bad{color:var(--bad)}.ok{color:var(--acid)}.warnText{color:var(--warn)}.mock{color:var(--pink);font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace}
@media(max-width:920px){.grid,.grid6,.controls{grid-template-columns:repeat(2,minmax(0,1fr))}.channels{grid-template-columns:1fr}header{display:block}.top{justify-content:flex-start;margin-top:10px}}
@media(max-width:620px){main{padding:10px}.controls,.grid,.grid6{grid-template-columns:1fr}.ch{grid-template-columns:48px 1fr 66px}.bar{grid-column:1/-1}.log{grid-template-columns:58px 48px 1fr}.log .hideMob{display:none}}
</style>
</head>
<body>
<main>
<header>
  <div><h1>Kosar RC Bench <span id="mockBadge" class="mock"></span></h1><p class="sub">ELRS / hover UART / debug cockpit</p></div>
  <div class="top">
    <div class="pill"><b id="link">...</b><span>crsf</span></div>
    <div class="pill"><b id="hover">...</b><span>hover</span></div>
    <div class="pill"><b id="armed">NO</b><span>armed</span></div>
    <div class="pill"><b id="source">rc</b><span>source</span></div>
  </div>
</header>
<nav class="tabs">
  <button class="tab active" data-tab="drive">Drive</button><button class="tab" data-tab="motors">Motors</button><button class="tab" data-tab="channels">Channels</button><button class="tab" data-tab="hoverTab">Hover</button><button class="tab" data-tab="logs">Logs</button><button class="tab" data-tab="network">Network</button>
</nav>
<section id="drive" class="panel active">
  <div class="grid">
    <div class="metric"><b id="profile">low</b><span>profile</span></div><div class="metric"><b id="speed">0</b><span>speed cmd</span></div><div class="metric"><b id="steer">0</b><span>steer cmd</span></div><div class="metric"><b id="battery">--.-</b><span>ESP battery V</span></div>
    <div class="metric"><b id="left">0</b><span>left motor</span></div><div class="metric"><b id="right">0</b><span>right motor</span></div><div class="metric"><b id="accel">0</b><span>accel/s</span></div><div class="metric"><b id="age">---</b><span>CRSF age ms</span></div>
  </div>
  <div class="controls">
    <label><span>web drive</span><input id="webEnabled" name="webEnabled" type="checkbox"></label><label><span>web arm</span><input id="webArm" name="webArm" type="checkbox"></label>
    <label><span>profile</span><select id="webMode"><option value="1">low</option><option value="2">mid</option><option value="3">max</option></select></label><button id="webStop" class="button danger" type="button">STOP</button>
    <label><span>speed <b id="webSpeedVal">0</b></span><input id="webSpeed" name="webSpeed" type="range" min="-100" max="100" value="0"></label><label><span>steer <b id="webSteerVal">0</b></span><input id="webSteer" name="webSteer" type="range" min="-100" max="100" value="0"></label>
    <div class="metric"><b id="webAge">---</b><span>web age ms</span></div><div class="metric"><b id="frames">0</b><span>frames</span></div>
  </div>
</section>
<section id="motors" class="panel">
  <div class="grid">
    <div class="metric"><b id="mLeftView">0</b><span>left command</span></div><div class="metric"><b id="mRightView">0</b><span>right command</span></div><div class="metric"><b id="motorAge">---</b><span>heartbeat age</span></div><div class="metric"><b id="crc">0</b><span>CRSF crc fail</span></div>
  </div>
  <div class="controls">
    <label><span>motor test</span><input id="motorEnabled" name="motorEnabled" type="checkbox"></label><label><span>test arm</span><input id="motorArm" name="motorArm" type="checkbox"></label>
    <label><span>direction</span><select id="motorDirection"><option value="0">forward</option><option value="1">reverse</option></select></label><button id="motorStop" class="button zero" type="button">ZERO</button>
    <label><span>left <b id="motorLeftVal">0</b></span><input id="motorLeft" name="motorLeft" type="range" min="0" max="100" value="0"></label><label><span>right <b id="motorRightVal">0</b></span><input id="motorRight" name="motorRight" type="range" min="0" max="100" value="0"></label><label><span>both <b id="motorBothVal">0</b></span><input id="motorBoth" name="motorBoth" type="range" min="0" max="100" value="0"></label><button id="motorHardStop" class="button danger" type="button">STOP</button>
  </div>
</section>
<section id="channels" class="panel"><div class="channels" id="channelsGrid"></div></section>
<section id="hoverTab" class="panel">
  <div class="grid grid6">
    <div class="metric"><b id="hAge">---</b><span>age ms</span></div><div class="metric"><b id="hcmd">0 / 0</b><span>cmd steer/speed</span></div><div class="metric"><b id="hspeed">0 / 0</b><span>rpm R/L</span></div><div class="metric"><b id="hbat">0</b><span>board bat</span></div><div class="metric"><b id="htemp">0</b><span>board temp</span></div><div class="metric"><b id="hcrc">0</b><span>hover crc</span></div>
    <div class="metric"><b id="htx">0</b><span>tx</span></div><div class="metric"><b id="hrx">0</b><span>rx</span></div><div class="metric"><b id="targetSpeed">0</b><span>target speed</span></div><div class="metric"><b id="targetSteer">0</b><span>target steer</span></div><div class="metric"><b id="maxCmd">0</b><span>max cmd</span></div><div class="metric"><b id="freeHeap">0</b><span>heap</span></div>
  </div>
</section>
<section id="logs" class="panel">
  <div class="logbar"><div><span class="chip" id="logSeq">seq 0</span> <span class="chip" id="logDropped">dropped 0</span></div><label style="display:flex;gap:8px;align-items:center"><input id="autoScroll" name="autoScroll" type="checkbox" checked> <span>autoscroll</span></label></div>
  <div id="logList" class="logs"></div>
</section>
<section id="network" class="panel">
  <div class="grid">
    <div class="metric"><b id="netMode">---</b><span>mode</span></div><div class="metric"><b id="netIp">---</b><span>ip</span></div><div class="metric"><b id="netSsid">---</b><span>ssid</span></div><div class="metric"><b id="netRssi">---</b><span>rssi</span></div>
    <div class="metric"><b id="netHost">kosar</b><span>hostname</span></div><div class="metric"><b id="netMdns">---</b><span>mDNS</span></div><div class="metric"><b id="uptime">0</b><span>uptime ms</span></div><div class="metric"><b id="heapNet">0</b><span>free heap</span></div>
  </div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
const isMock=new URLSearchParams(location.search).get('mock')==='1';
if(isMock)$('mockBadge').textContent='// MOCK';
document.querySelectorAll('.tab').forEach(btn=>btn.addEventListener('click',()=>{document.querySelectorAll('.tab,.panel').forEach(x=>x.classList.remove('active'));btn.classList.add('active');$(btn.dataset.tab).classList.add('active')}));
const root=$('channelsGrid'),webEnabled=$('webEnabled'),webArm=$('webArm'),webMode=$('webMode'),webSpeed=$('webSpeed'),webSteer=$('webSteer'),webSpeedVal=$('webSpeedVal'),webSteerVal=$('webSteerVal');
const motorEnabled=$('motorEnabled'),motorArm=$('motorArm'),motorDirection=$('motorDirection'),motorLeft=$('motorLeft'),motorRight=$('motorRight'),motorBoth=$('motorBoth'),motorLeftVal=$('motorLeftVal'),motorRightVal=$('motorRightVal'),motorBothVal=$('motorBothVal');
let lastWebSend=0,lastMotorSend=0,lastLogSeq=0,mockFrames=420,mockT=0,controlToken='',controlReady=false;
let webBusy=false,webPending=false,motorBusy=false,motorPending=false,tickBusy=false,logsBusy=false,networkBusy=false;
const names=["Roll","Pitch","Throttle","Yaw","Arm","Mode (3x)","Aux 3","Return home","Beep","Buttons (6x)","Aux 7","Aux 8","Aux 9","Aux 10","Aux 11","Aux 12"];
for(let i=0;i<16;i++){const saved=localStorage.getItem('kosar.ch'+i)||names[i]||('CH '+(i+1));const row=document.createElement('div');row.className='ch';row.innerHTML=`<div class="idx">CH${i+1}</div><input id="chLabel${i+1}" name="chLabel${i+1}" aria-label="CH${i+1} label" class="label" autocomplete="off" value="${saved.replaceAll('"','&quot;')}"><div class="bar"><div class="fill"></div></div><div class="value">----</div>`;row.querySelector('input').addEventListener('input',e=>localStorage.setItem('kosar.ch'+i,e.target.value));root.appendChild(row)}
document.querySelectorAll('input,select').forEach(el=>el.setAttribute('autocomplete','off'));
function resetUiSafe(){webEnabled.checked=false;webArm.checked=false;webMode.value='1';webSpeed.value=0;webSteer.value=0;webSpeedVal.textContent='0';webSteerVal.textContent='0';motorEnabled.checked=false;motorArm.checked=false;motorDirection.value='0';motorLeft.value=0;motorRight.value=0;motorBoth.value=0;motorLeftVal.textContent='0';motorRightVal.textContent='0';motorBothVal.textContent='0'}
resetUiSafe();
function api(path,opts={}){if(isMock)return Promise.resolve(mockApi(path,opts));return fetch(path,{cache:'no-store',...opts}).then(r=>{if(!r.ok)throw new Error('HTTP '+r.status);return r.json()})}
function mockApi(path,opts={}){mockT+=1;if(path.startsWith('/api/session'))return{control_token:'mock'};if(path.startsWith('/api/network'))return{mode:'ap',status:'AP',hostname:'kosar',ip:'192.168.4.1',ssid:'',rssi:0,ap_ssid:'Kosar-RC',mdns:true,uptime_ms:Date.now()%999999,free_heap:181240};if(path.startsWith('/api/logs')){let ev=[];for(let i=0;i<3;i++){lastLogSeq++;ev.push({seq:lastLogSeq,ms:Date.now()%999999,level:i==1?'warn':'info',type:['boot','wifi','hover'][i],msg:['bench mock alive','AP fallback Kosar-RC','hover link up'][i],a:i,b:0})}return{seq_latest:lastLogSeq,dropped:0,events:ev}};const s=Math.round(Math.sin(mockT/8)*420),st=Math.round(Math.cos(mockT/11)*260);return{link:true,age_ms:18,frames:mockFrames++,crc_fail:2,bad_frames:0,battery_v:38.4,uptime_ms:Date.now()%999999,free_heap:181240,log_seq:lastLogSeq,last_log_seq:lastLogSeq,web_drive:{enabled:webEnabled.checked,armed:webArm.checked,active:webEnabled.checked,age_ms:42,speed:+webSpeed.value,steer:+webSteer.value,mode:+webMode.value},motor_test:{enabled:motorEnabled.checked,armed:motorArm.checked,active:motorEnabled.checked,reverse:motorDirection.value==='1',age_ms:35,left:+motorLeft.value,right:+motorRight.value},hover:{link:true,age_ms:22,tx:mockFrames*2,rx:mockFrames,crc_fail:1,cmd1:st,cmd2:s,speed_r:s+7,speed_l:s-9,bat:384,temp:32},control:{armed:webArm.checked||motorArm.checked,source:motorEnabled.checked?'motor':(webEnabled.checked?'web':'rc'),mode:+webMode.value,profile:['low','mid','max'][+webMode.value-1],max:[250,600,1000][+webMode.value-1],accel:800,target_speed:s,target_steer:st,speed:s,steer:st,left:s+st,right:s-st},channels:Array.from({length:16},(_,i)=>({raw:992,us:i<4?1500+Math.round(Math.sin(mockT/10+i)*330):(i==4?1000:i==5?1500:1000)}))}}
function tokenParams(params){params.set('token',controlToken);return params}
function controlUrl(path,params){return path+'?'+tokenParams(params).toString()}
async function postControl(path,params,opts={}){if(isMock)return mockApi(path+'?'+params.toString(),{method:'POST'});if(!controlReady)return;return api(controlUrl(path,params),{method:'POST',headers:{'X-Control-Token':controlToken},keepalive:!!opts.keepalive})}
function webParams(){return new URLSearchParams({enabled:webEnabled.checked?'1':'0',armed:webArm.checked?'1':'0',mode:webMode.value,speed:webSpeed.value,steer:webSteer.value})}
function motorParams(){return new URLSearchParams({enabled:motorEnabled.checked?'1':'0',armed:motorArm.checked?'1':'0',reverse:motorDirection.value,left:motorLeft.value,right:motorRight.value})}
function stopWebDrive(){webEnabled.checked=false;webArm.checked=false;webSpeed.value=0;webSteer.value=0;webSpeedVal.textContent='0';webSteerVal.textContent='0';sendWebDrive(true)}
function disableWebDrive(){webEnabled.checked=false;webArm.checked=false;webSpeed.value=0;webSteer.value=0;sendWebDrive(true)}
async function sendWebDrive(force=false){const now=Date.now();if(!force&&now-lastWebSend<200)return;if(webBusy){webPending=webPending||force;return}lastWebSend=now;webSpeedVal.textContent=webSpeed.value;webSteerVal.textContent=webSteer.value;webBusy=true;try{await postControl('/api/webdrive',webParams())}catch(e){}finally{webBusy=false;if(webPending){webPending=false;sendWebDrive(true)}}}
function zeroMotorTest(){motorArm.checked=false;motorLeft.value=0;motorRight.value=0;motorBoth.value=0;motorLeftVal.textContent='0';motorRightVal.textContent='0';motorBothVal.textContent='0';sendMotorTest(true)}
function disableMotorTest(){motorEnabled.checked=false;motorArm.checked=false;motorLeft.value=0;motorRight.value=0;motorBoth.value=0;sendMotorTest(true)}
async function sendMotorTest(force=false){const now=Date.now();if(!force&&now-lastMotorSend<200)return;if(motorBusy){motorPending=motorPending||force;return}lastMotorSend=now;motorLeftVal.textContent=motorLeft.value;motorRightVal.textContent=motorRight.value;motorBothVal.textContent=motorBoth.value;motorBusy=true;try{await postControl('/api/motortest',motorParams())}catch(e){}finally{motorBusy=false;if(motorPending){motorPending=false;sendMotorTest(true)}}}
$('webStop').addEventListener('click',stopWebDrive);$('motorStop').addEventListener('click',zeroMotorTest);$('motorHardStop').addEventListener('click',()=>{motorEnabled.checked=false;zeroMotorTest()});
webEnabled.addEventListener('input',()=>{if(webEnabled.checked)disableMotorTest();sendWebDrive(true)});[webArm,webMode,webSpeed,webSteer].forEach(el=>el.addEventListener('input',()=>sendWebDrive(true)));
motorBoth.addEventListener('input',()=>{motorLeft.value=motorBoth.value;motorRight.value=motorBoth.value;sendMotorTest(true)});motorEnabled.addEventListener('input',()=>{if(motorEnabled.checked)disableWebDrive();sendMotorTest(true)});[motorArm,motorDirection,motorLeft,motorRight].forEach(el=>el.addEventListener('input',()=>sendMotorTest(true)));
function put(id,v){$(id).textContent=v}
async function tick(){if(tickBusy)return;tickBusy=true;try{const d=await api('/api/channels');document.body.classList.toggle('dead',!d.link);put('link',d.link?'OK':'NO');put('hover',d.hover.link?'OK':'NO');put('age',d.age_ms);put('frames',d.frames);put('crc',d.crc_fail);put('armed',d.control.armed?'YES':'NO');put('source',d.control.source);put('profile',d.control.profile);put('accel',d.control.accel);put('speed',d.control.speed);put('steer',d.control.steer);put('left',d.control.left);put('right',d.control.right);put('battery',Number(d.battery_v).toFixed(1));put('webAge',d.web_drive.age_ms);put('motorAge',d.motor_test.age_ms);put('mLeftView',d.motor_test.left);put('mRightView',d.motor_test.right);put('hAge',d.hover.age_ms);put('hcmd',d.hover.cmd1+' / '+d.hover.cmd2);put('hspeed',d.hover.speed_r+' / '+d.hover.speed_l);put('hbat',d.hover.bat);put('htemp',d.hover.temp);put('hcrc',d.hover.crc_fail);put('htx',d.hover.tx);put('hrx',d.hover.rx);put('targetSpeed',d.control.target_speed);put('targetSteer',d.control.target_steer);put('maxCmd',d.control.max);put('freeHeap',d.free_heap||0);d.channels.forEach((ch,i)=>{const row=root.children[i];const pct=Math.max(0,Math.min(100,(ch.us-988)/(2012-988)*100));row.querySelector('.fill').style.width=pct+'%';row.querySelector('.value').textContent=ch.us+' us'})}catch(e){document.body.classList.add('dead');put('link','NO')}finally{tickBusy=false}}
async function pollLogs(){if(logsBusy)return;logsBusy=true;try{const d=await api('/api/logs?since='+lastLogSeq+'&limit=40');lastLogSeq=d.seq_latest||lastLogSeq;put('logSeq','seq '+lastLogSeq);put('logDropped','dropped '+(d.dropped||0));const list=$('logList');(d.events||[]).forEach(ev=>{const row=document.createElement('div');row.className='log '+ev.level;row.innerHTML=`<span>${ev.ms}</span><span>${ev.level}</span><span class="hideMob">${ev.type}</span><span>${ev.msg}</span><span class="hideMob">${ev.a??''}</span><span class="hideMob">${ev.b??''}</span>`;list.appendChild(row);while(list.children.length>180)list.removeChild(list.firstChild)});if($('autoScroll').checked)list.scrollTop=list.scrollHeight}catch(e){}finally{logsBusy=false}}
async function pollNetwork(){if(networkBusy)return;networkBusy=true;try{const d=await api('/api/network');put('netMode',d.mode);put('netIp',d.ip);put('netSsid',d.mode==='ap'?d.ap_ssid:d.ssid);put('netRssi',d.mode==='sta'?d.rssi:'---');put('netHost',d.hostname);put('netMdns',d.mdns?'OK':'NO');put('uptime',d.uptime_ms);put('heapNet',d.free_heap)}catch(e){}finally{networkBusy=false}}
function safeOffFetchKeepalive(){try{fetch(controlUrl('/api/webdrive',webParams()),{method:'POST',keepalive:true,cache:'no-store'});fetch(controlUrl('/api/motortest',motorParams()),{method:'POST',keepalive:true,cache:'no-store'})}catch(_){}}
function safeOffKeepalive(){resetUiSafe();if(isMock||!controlReady)return;try{const webOk=navigator.sendBeacon(controlUrl('/api/webdrive',webParams()));const motorOk=navigator.sendBeacon(controlUrl('/api/motortest',motorParams()));if(!webOk||!motorOk)safeOffFetchKeepalive()}catch(e){safeOffFetchKeepalive()}}
async function safeOffStartup(){resetUiSafe();try{await postControl('/api/webdrive',webParams());await postControl('/api/motortest',motorParams())}catch(e){}}
function startPolling(){setInterval(()=>{if(webEnabled.checked)sendWebDrive(false)},200);setInterval(()=>{if(motorEnabled.checked)sendMotorTest(false)},200);setInterval(tick,200);setInterval(pollLogs,1000);setInterval(pollNetwork,3000);tick();pollLogs();pollNetwork()}
async function initSession(){try{const d=await api('/api/session');controlToken=d.control_token||'mock';controlReady=!!controlToken;await safeOffStartup()}catch(e){controlReady=isMock;controlToken=isMock?'mock':''}startPolling()}
window.addEventListener('pagehide',safeOffKeepalive);window.addEventListener('beforeunload',safeOffKeepalive);
initSession();
</script>
</body>
</html>
)HTML";

void copySmall(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) {
    return;
  }
  size_t i = 0;
  for (; i + 1 < destSize && src[i] != '\0'; i++) {
    dest[i] = src[i];
  }
  dest[i] = '\0';
}

void logEvent(const char *level, const char *type, const char *msg, int32_t a = 0, int32_t b = 0) {
  LogEvent &event = logEvents[logWriteIndex];
  event.seq = ++logSeqLatest;
  event.ms = millis();
  copySmall(event.level, sizeof(event.level), level);
  copySmall(event.type, sizeof(event.type), type);
  copySmall(event.msg, sizeof(event.msg), msg);
  event.a = a;
  event.b = b;

  logWriteIndex = (logWriteIndex + 1) % LOG_EVENT_CAPACITY;
  if (logCount < LOG_EVENT_CAPACITY) {
    logCount++;
  } else {
    logDropped++;
  }

  Serial.print("[");
  Serial.print(level);
  Serial.print("] ");
  Serial.print(type);
  Serial.print(": ");
  Serial.println(msg);
}

void appendJsonEscaped(String &json, const char *value) {
  json += '"';
  for (size_t i = 0; value[i] != '\0'; i++) {
    const char c = value[i];
    if (c == '"' || c == '\\') {
      json += '\\';
      json += c;
    } else if (static_cast<uint8_t>(c) < 0x20) {
      json += ' ';
    } else {
      json += c;
    }
  }
  json += '"';
}

void generateControlToken() {
  snprintf(
    controlToken,
    sizeof(controlToken),
    "%08lx%08lx",
    static_cast<unsigned long>(esp_random()),
    static_cast<unsigned long>(esp_random())
  );
}

bool hasValidControlToken() {
  String token;
  if (server.hasArg("token")) {
    token = server.arg("token");
  } else if (server.hasArg("t")) {
    token = server.arg("t");
  } else {
    token = server.header(CONTROL_TOKEN_HEADER);
  }

  return token.length() > 0 && token == controlToken;
}

void sendJsonError(int code, const char *error) {
  String json;
  json.reserve(48);
  json += "{\"ok\":false,\"error\":";
  appendJsonEscaped(json, error);
  json += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", json);
}

bool requirePostAndControlToken() {
  if (server.method() != HTTP_POST) {
    server.sendHeader("Allow", "POST");
    sendJsonError(405, "method_not_allowed");
    return false;
  }
  if (!hasValidControlToken()) {
    sendJsonError(403, "bad_control_token");
    return false;
  }
  return true;
}

bool shouldLogCommandChange(uint32_t &lastLogMs) {
  const uint32_t now = millis();
  if (lastLogMs != 0 && now - lastLogMs < COMMAND_LOG_MIN_INTERVAL_MS) {
    return false;
  }
  lastLogMs = now;
  return true;
}

bool hoverLinkNow() {
  const uint32_t hoverAgeMs = lastHoverFeedbackMs == 0 ? 999999 : millis() - lastHoverFeedbackMs;
  return lastHoverFeedbackMs != 0 && hoverAgeMs < 500;
}

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

  if (webDrive.enabled && webDrive.lastUpdateMs != 0 &&
      now - webDrive.lastUpdateMs >= WEB_DRIVE_TIMEOUT_MS) {
    webDrive.enabled = false;
    webDrive.armed = false;
    webDrive.speed = 0;
    webDrive.steer = 0;
    forceCoastUntilMs = now + COAST_HOLD_MS;
    logEvent("warn", "failsafe", "webdrive heartbeat lost", now - webDrive.lastUpdateMs, 0);
  }

  if (motorTest.enabled && motorTest.lastUpdateMs != 0 &&
      now - motorTest.lastUpdateMs >= MOTOR_TEST_TIMEOUT_MS) {
    motorTest.enabled = false;
    motorTest.armed = false;
    motorTest.left = 0;
    motorTest.right = 0;
    forceCoastUntilMs = now + COAST_HOLD_MS;
    logEvent("warn", "failsafe", "motortest heartbeat lost", now - motorTest.lastUpdateMs, 0);
  }

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

  if (forceCoastUntilMs != 0 && now < forceCoastUntilMs) {
    driveMix.armed = false;
    driveMix.targetSpeed = 0;
    driveMix.targetSteer = 0;
    driveMix.speed = 0;
    driveMix.steer = 0;
    driveMix.left = 0;
    driveMix.right = 0;
  } else if (forceCoastUntilMs != 0 && now >= forceCoastUntilMs) {
    forceCoastUntilMs = 0;
  }
}

void updateTransitionLogs() {
  const bool crsfLink = driveMix.link;
  const bool hoverLink = hoverLinkNow();

  if (crsfLink != prevCrsfLink) {
    logEvent(crsfLink ? "info" : "warn", "link", crsfLink ? "CRSF link up" : "CRSF link down", driveMix.ageMs, 0);
    if (!crsfLink && prevArmed && !prevWebActive && !prevMotorActive) {
      logEvent("warn", "failsafe", "RC link lost while armed", driveMix.ageMs, 0);
      forceCoastUntilMs = millis() + COAST_HOLD_MS;
    }
    prevCrsfLink = crsfLink;
  }

  if (hoverLink != prevHoverLink) {
    logEvent(hoverLink ? "info" : "warn", "hover", hoverLink ? "hover feedback up" : "hover feedback lost", lastHoverFeedbackMs, 0);
    prevHoverLink = hoverLink;
  }

  if (driveMix.armed != prevArmed) {
    logEvent(driveMix.armed ? "info" : "info", driveMix.armed ? "arm" : "disarm", driveMix.armed ? "drive armed" : "drive disarmed", driveMix.mode, 0);
    if (!driveMix.armed) {
      logEvent("info", "coast", "COAST/OFF command active", 0, 0);
    }
    prevArmed = driveMix.armed;
  }

  if (driveMix.webActive != prevWebActive) {
    logEvent(driveMix.webActive ? "info" : "info", "webdrive", driveMix.webActive ? "webdrive active" : "webdrive inactive", webDrive.enabled ? 1 : 0, webDrive.armed ? 1 : 0);
    prevWebActive = driveMix.webActive;
  }

  if (driveMix.motorTestActive != prevMotorActive) {
    logEvent(driveMix.motorTestActive ? "info" : "info", "motortest", driveMix.motorTestActive ? "motortest active" : "motortest inactive", motorTest.enabled ? 1 : 0, motorTest.armed ? 1 : 0);
    prevMotorActive = driveMix.motorTestActive;
  }
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

void handleApiSession() {
  String json;
  json.reserve(48);
  json += "{\"control_token\":";
  appendJsonEscaped(json, controlToken);
  json += '}';
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleMutationGetRejected() {
  server.sendHeader("Allow", "POST");
  sendJsonError(405, "method_not_allowed");
}

void handleWebDrive() {
  if (!requirePostAndControlToken()) {
    return;
  }

  const bool wasEnabled = webDrive.enabled;
  const bool wasArmed = webDrive.armed;
  const uint8_t wasMode = webDrive.mode;
  const int16_t wasSpeed = webDrive.speed;
  const int16_t wasSteer = webDrive.steer;
  const bool wasMotorEnabled = motorTest.enabled;
  const bool wasMotorArmed = motorTest.armed;

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
    if (wasEnabled || wasArmed) {
      forceCoastUntilMs = millis() + COAST_HOLD_MS;
    }
  } else {
    motorTest.enabled = false;
    motorTest.armed = false;
    motorTest.left = 0;
    motorTest.right = 0;
  }

  webDrive.lastUpdateMs = millis();
  if (webDrive.enabled != wasEnabled) {
    logEvent("info", "webdrive", webDrive.enabled ? "webdrive enabled" : "webdrive disabled", webDrive.speed, webDrive.steer);
  }
  if (webDrive.armed != wasArmed) {
    logEvent("info", webDrive.armed ? "arm" : "disarm", webDrive.armed ? "webdrive armed" : "webdrive disarmed", webDrive.mode, 0);
  }
  if (webDrive.enabled && (webDrive.mode != wasMode || webDrive.speed != wasSpeed || webDrive.steer != wasSteer) &&
      shouldLogCommandChange(lastWebCommandLogMs)) {
    logEvent("info", "webdrive", "webdrive command changed", webDrive.speed, webDrive.steer);
  }
  if ((wasMotorEnabled || wasMotorArmed) && webDrive.enabled) {
    logEvent("info", "motortest", "motortest disabled by webdrive", 0, 0);
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleMotorTest() {
  if (!requirePostAndControlToken()) {
    return;
  }

  const bool wasEnabled = motorTest.enabled;
  const bool wasArmed = motorTest.armed;
  const bool wasReverse = motorTest.reverse;
  const int16_t wasLeft = motorTest.left;
  const int16_t wasRight = motorTest.right;
  const bool wasWebEnabled = webDrive.enabled;
  const bool wasWebArmed = webDrive.armed;

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
    if (wasEnabled || wasArmed) {
      forceCoastUntilMs = millis() + COAST_HOLD_MS;
    }
  } else {
    webDrive.enabled = false;
    webDrive.armed = false;
    webDrive.speed = 0;
    webDrive.steer = 0;
  }

  motorTest.lastUpdateMs = millis();
  if (motorTest.enabled != wasEnabled) {
    logEvent("info", "motortest", motorTest.enabled ? "motortest enabled" : "motortest disabled", motorTest.left, motorTest.right);
  }
  if (motorTest.armed != wasArmed) {
    logEvent("info", motorTest.armed ? "arm" : "disarm", motorTest.armed ? "motortest armed" : "motortest disarmed", motorTest.left, motorTest.right);
  }
  if (motorTest.enabled && (motorTest.reverse != wasReverse || motorTest.left != wasLeft || motorTest.right != wasRight) &&
      shouldLogCommandChange(lastMotorCommandLogMs)) {
    logEvent("info", "motortest", "motortest command changed", motorTest.left, motorTest.right);
  }
  if ((wasWebEnabled || wasWebArmed) && motorTest.enabled) {
    logEvent("info", "webdrive", "webdrive disabled by motortest", 0, 0);
  }
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
  command.steer = driveMix.armed ? driveMix.steer : HOVER_COAST_COMMAND;
  command.speed = driveMix.armed ? driveMix.speed : HOVER_COAST_COMMAND;
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
  json += ",\"uptime_ms\":";
  json += millis();
  json += ",\"free_heap\":";
  json += ESP.getFreeHeap();
  json += ",\"log_seq\":";
  json += logSeqLatest;
  json += ",\"last_log_seq\":";
  json += logSeqLatest;
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

void handleApiLogs() {
  uint32_t since = 0;
  int limit = 40;
  if (server.hasArg("since")) {
    since = static_cast<uint32_t>(server.arg("since").toInt());
  }
  if (server.hasArg("limit")) {
    limit = constrain(server.arg("limit").toInt(), 1, 80);
  }

  const uint32_t earliestSeq = logCount == 0 ? 0 : (logSeqLatest - logCount + 1);
  String json;
  json.reserve(4200);
  json += "{\"seq_latest\":";
  json += logSeqLatest;
  json += ",\"dropped\":";
  json += logDropped;
  json += ",\"events\":[";

  bool first = true;
  int emitted = 0;
  for (uint32_t seq = max<uint32_t>(since + 1, earliestSeq); seq <= logSeqLatest && emitted < limit; seq++) {
    const size_t index = (logWriteIndex + LOG_EVENT_CAPACITY - (logSeqLatest - seq + 1)) % LOG_EVENT_CAPACITY;
    const LogEvent &event = logEvents[index];
    if (event.seq != seq) {
      continue;
    }
    if (!first) {
      json += ',';
    }
    first = false;
    emitted++;
    json += "{\"seq\":";
    json += event.seq;
    json += ",\"ms\":";
    json += event.ms;
    json += ",\"level\":";
    appendJsonEscaped(json, event.level);
    json += ",\"type\":";
    appendJsonEscaped(json, event.type);
    json += ",\"msg\":";
    appendJsonEscaped(json, event.msg);
    json += ",\"a\":";
    json += event.a;
    json += ",\"b\":";
    json += event.b;
    json += '}';
  }
  json += "]}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void handleApiNetwork() {
  String ip = wifiApMode ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
  String ssid = wifiApMode ? "" : WiFi.SSID();
  String status = wifiApMode ? "AP" : (WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");

  String json;
  json.reserve(420);
  json += "{\"mode\":\"";
  json += wifiApMode ? "ap" : "sta";
  json += "\",\"status\":";
  appendJsonEscaped(json, status.c_str());
  json += ",\"hostname\":";
  appendJsonEscaped(json, HOSTNAME);
  json += ",\"ip\":";
  appendJsonEscaped(json, ip.c_str());
  json += ",\"ssid\":";
  appendJsonEscaped(json, ssid.c_str());
  json += ",\"rssi\":";
  json += wifiApMode ? 0 : WiFi.RSSI();
  json += ",\"ap_ssid\":";
  appendJsonEscaped(json, wifiApMode ? AP_SSID : "");
  json += ",\"mdns\":";
  json += mdnsStarted ? "true" : "false";
  json += ",\"uptime_ms\":";
  json += millis();
  json += ",\"free_heap\":";
  json += ESP.getFreeHeap();
  json += '}';

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void startWifi() {
  if (strlen(KOSAR_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    wifiApMode = false;
    WiFi.begin(KOSAR_WIFI_SSID, KOSAR_WIFI_PASS);

    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(KOSAR_WIFI_SSID);
    logEvent("info", "wifi", "STA connect start", 0, 0);

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 10000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("STA IP: ");
      Serial.println(WiFi.localIP());
      logEvent("info", "wifi", "STA connected", WiFi.RSSI(), 0);
      return;
    }

    Serial.println("Home Wi-Fi failed, falling back to AP");
    logEvent("warn", "wifi", "STA connect failed", 0, 0);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  wifiApMode = true;
  logEvent("warn", "ap", "AP fallback started", 0, 0);

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
  logEvent("info", "boot", "Kosar RC Bench boot", 0, 0);
  generateControlToken();

  analogReadResolution(12);
  analogSetPinAttenuation(VBAT_ADC_PIN, ADC_11db);

  crsfSerial.begin(CRSF_BAUD, SERIAL_8N1, CRSF_RX_PIN, CRSF_TX_PIN);
  hoverSerial.begin(HOVER_BAUD, SERIAL_8N1, HOVER_RX_PIN, HOVER_TX_PIN);

  startWifi();

  server.collectHeaders(CONTROL_HEADER_KEYS, 1);
  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/session", HTTP_GET, handleApiSession);
  server.on("/api/channels", HTTP_GET, handleApiChannels);
  server.on("/api/logs", HTTP_GET, handleApiLogs);
  server.on("/api/network", HTTP_GET, handleApiNetwork);
  server.on("/api/webdrive", HTTP_GET, handleMutationGetRejected);
  server.on("/api/webdrive", HTTP_POST, handleWebDrive);
  server.on("/api/motortest", HTTP_GET, handleMutationGetRejected);
  server.on("/api/motortest", HTTP_POST, handleMotorTest);
  server.begin();

  if (MDNS.begin(HOSTNAME)) {
    mdnsStarted = true;
    Serial.println("mDNS: http://kosar.local/");
    logEvent("info", "wifi", "mDNS started", 0, 0);
  } else {
    logEvent("warn", "wifi", "mDNS failed", 0, 0);
  }

  Serial.print("CRSF RX pin: GPIO");
  Serial.println(CRSF_RX_PIN);
  Serial.print("Hover RX/TX pins: GPIO");
  Serial.print(HOVER_RX_PIN);
  Serial.print("/GPIO");
  Serial.println(HOVER_TX_PIN);
  logEvent("warn", "link", "CRSF link down initial", 0, 0);
  logEvent("warn", "hover", "hover feedback absent initial", 0, 0);
}

void loop() {
  readCrsf();
  updateDriveMix();
  updateTransitionLogs();
  sendHoverCommand();
  readHoverFeedback();
  updateBatteryVoltage();
  server.handleClient();
}
