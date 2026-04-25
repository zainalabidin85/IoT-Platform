// =============================================================
//  ESP32 Soil Sensor Dashboard v2
//  Sensor : CWT-SOIL-NPKPHCTH-S via MAX485 (RS485 / Modbus RTU)
//  New    : WiFi config portal, Basic Auth, MQTT + HA Discovery
//  Author : Dr. Zainal Abidin Arsat
// =============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <ModbusMaster.h>
#include <Preferences.h>
#include <PubSubClient.h>

// ── Pins ────────────────────────────────────────────────────
#define RX2_PIN    16
#define TX2_PIN    17
#define DE_RE_PIN   4
#define BOOT_BTN    0   // Hold on power-up → force config portal

// ── RS485 / Modbus ──────────────────────────────────────────
ModbusMaster node;
void preTransmission()  { digitalWrite(DE_RE_PIN, HIGH); }
void postTransmission() { digitalWrite(DE_RE_PIN, LOW);  }

// ── NVS ─────────────────────────────────────────────────────
Preferences prefs;

// ── Runtime config (loaded from NVS) ────────────────────────
char wifiSSID[64]  = "";
char wifiPass[64]  = "";
char mqttHost[64]  = "";
uint16_t mqttPort  = 1883;
char mqttUser[32]  = "";
char mqttPass[32]  = "";
char webUser[32]   = "admin";
char webPass[32]   = "admin";
char deviceId[32]  = "soil_npk";   // MQTT topic prefix + HA unique ID base

// ── Sensor values ────────────────────────────────────────────
float N=-1, P=-1, K=-1, pH=-1, EC=-1, Moist=-1, Temp=-1;
float offset_N=0, offset_P=0, offset_K=0;
float offset_pH=0, offset_EC=0, offset_Moist=0, offset_Temp=0;
unsigned long lastRead = 0;
const unsigned long READ_INTERVAL = 2000;

// ── MQTT ─────────────────────────────────────────────────────
WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);
#include <HTTPUpdate.h>
static const char* FW_VERSION = "1.0.0";
static String _otaTopic;

static void performOTA(const String& url) {
  Serial.printf("[OTA] Starting from: %s\n", url.c_str());
  WiFiClient httpClient;
  httpUpdate.rebootOnUpdate(true);
  auto ret = httpUpdate.update(httpClient, url);
  if (ret == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
}

static void mqttCallback(char* topic, byte* payload, unsigned int len) {
  if (String(topic) != _otaTopic) return;
  String msg; msg.reserve(len);
  for (unsigned i = 0; i < len; i++) msg += (char)payload[i];
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, msg) != DeserializationError::Ok) return;
  const char* ver = doc["version"];
  const char* url = doc["url"];
  if (url && ver && String(ver) != FW_VERSION) {
    Serial.printf("[OTA] New fw %s (have %s)\n", ver, FW_VERSION);
    performOTA(url);
  }
}
unsigned long lastMqttPublish  = 0;
unsigned long lastMqttAttempt  = 0;
const unsigned long MQTT_INTERVAL = 30000;

// ── Web server ───────────────────────────────────────────────
WebServer server(80);
bool configMode = false;

// =============================================================
//  NVS helpers
// =============================================================

void loadConfig() {
    prefs.begin("wifi", true);
    strlcpy(wifiSSID, prefs.getString("ssid", "").c_str(), sizeof(wifiSSID));
    strlcpy(wifiPass, prefs.getString("pass", "").c_str(), sizeof(wifiPass));
    prefs.end();

    prefs.begin("mqtt_cfg", true);
    strlcpy(mqttHost, prefs.getString("host", "").c_str(), sizeof(mqttHost));
    mqttPort = (uint16_t)prefs.getUInt("port", 1883);
    strlcpy(mqttUser, prefs.getString("user", "").c_str(), sizeof(mqttUser));
    strlcpy(mqttPass, prefs.getString("pass", "").c_str(), sizeof(mqttPass));
    prefs.end();

    prefs.begin("auth", true);
    strlcpy(webUser,  prefs.getString("user",  "admin").c_str(), sizeof(webUser));
    strlcpy(webPass,  prefs.getString("pass",  "admin").c_str(), sizeof(webPass));
    strlcpy(deviceId, prefs.getString("devid", "soil_npk").c_str(), sizeof(deviceId));
    prefs.end();

    prefs.begin("offsets", true);
    offset_N     = prefs.getFloat("n",     0);
    offset_P     = prefs.getFloat("p",     0);
    offset_K     = prefs.getFloat("k",     0);
    offset_pH    = prefs.getFloat("ph",    0);
    offset_EC    = prefs.getFloat("ec",    0);
    offset_Moist = prefs.getFloat("moist", 0);
    offset_Temp  = prefs.getFloat("temp",  0);
    prefs.end();
}

void saveWifiConfig(const char* ssid, const char* pass) {
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    if (strlen(pass) > 0) prefs.putString("pass", pass);
    prefs.end();
}

void saveMqttConfig(const char* host, uint16_t port,
                    const char* user, const char* pass) {
    prefs.begin("mqtt_cfg", false);
    prefs.putString("host", host);
    prefs.putUInt("port", port);
    prefs.putString("user", user);
    if (strlen(pass) > 0) prefs.putString("pass", pass);
    prefs.end();
}

void saveAuthConfig(const char* user, const char* pass, const char* devid) {
    prefs.begin("auth", false);
    prefs.putString("user",  user);
    if (strlen(pass) > 0) prefs.putString("pass", pass);
    prefs.putString("devid", devid);
    prefs.end();
}

void saveOffsets() {
    prefs.begin("offsets", false);
    prefs.putFloat("n",     offset_N);
    prefs.putFloat("p",     offset_P);
    prefs.putFloat("k",     offset_K);
    prefs.putFloat("ph",    offset_pH);
    prefs.putFloat("ec",    offset_EC);
    prefs.putFloat("moist", offset_Moist);
    prefs.putFloat("temp",  offset_Temp);
    prefs.end();
}

void clearAllConfig() {
    const char* ns[] = {"wifi", "mqtt_cfg", "auth", "offsets"};
    for (auto n : ns) { prefs.begin(n, false); prefs.clear(); prefs.end(); }
}

// =============================================================
//  MQTT / Home Assistant
// =============================================================

void mqttPublishState() {
    char telTopic[80];
    snprintf(telTopic, sizeof(telTopic), "%s/telemetry", deviceId);

    char payload[200];
    snprintf(payload, sizeof(payload),
        "{\"N\":%.1f,\"P\":%.1f,\"K\":%.1f,"
        "\"pH\":%.2f,\"EC\":%.0f,"
        "\"Moist\":%.1f,\"Temp\":%.1f}",
        N, P, K, pH, EC, Moist, Temp);

    mqtt.publish(telTopic, payload);

    // Individual scalar topics
    char buf[24];
    snprintf(buf, sizeof(buf), "%.1f", N);    mqtt.publish((String(deviceId) + "/nitrogen").c_str(),    buf);
    snprintf(buf, sizeof(buf), "%.1f", P);    mqtt.publish((String(deviceId) + "/phosphorus").c_str(),  buf);
    snprintf(buf, sizeof(buf), "%.1f", K);    mqtt.publish((String(deviceId) + "/potassium").c_str(),   buf);
    snprintf(buf, sizeof(buf), "%.2f", pH);   mqtt.publish((String(deviceId) + "/ph").c_str(),          buf);
    snprintf(buf, sizeof(buf), "%.0f", EC);   mqtt.publish((String(deviceId) + "/ec").c_str(),          buf);
    snprintf(buf, sizeof(buf), "%.1f", Moist);mqtt.publish((String(deviceId) + "/moisture").c_str(),    buf);
    snprintf(buf, sizeof(buf), "%.1f", Temp); mqtt.publish((String(deviceId) + "/temperature").c_str(), buf);
}

void mqttReconnect() {
    if (strlen(mqttHost) == 0 || mqtt.connected()) return;

    Serial.print("Connecting to MQTT...");
    char clientId[48];
    snprintf(clientId, sizeof(clientId), "esp32_%s", deviceId);

    char willTopic[80];
    snprintf(willTopic, sizeof(willTopic), "%s/status", deviceId);

    mqtt.setCallback(mqttCallback);

    bool ok = (strlen(mqttUser) > 0)
              ? mqtt.connect(clientId, mqttUser, mqttPass, willTopic, 1, true, "offline")
              : mqtt.connect(clientId, willTopic, 1, true, "offline");

    if (ok) {
        Serial.println(" OK");
        mqtt.publish(willTopic, "online", true);
        _otaTopic = String(deviceId) + "/ota/command";
        mqtt.subscribe(_otaTopic.c_str());
    } else {
        Serial.printf(" failed (rc=%d)\n", mqtt.state());
    }
}

// =============================================================
//  HTML Pages
// =============================================================

// ── Config portal (AP mode, no auth) ────────────────────────
const char CONFIG_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NPK Sensor Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:16px}
h1{color:#58a6ff;font-size:1.15rem;margin-bottom:20px}
h2{color:#8b949e;font-size:0.78rem;text-transform:uppercase;letter-spacing:0.05em;margin:0 0 10px}
.card{background:#161b22;border:1px solid #21262d;border-radius:10px;padding:16px;margin-bottom:14px}
label{display:block;font-size:0.75rem;color:#8b949e;margin-bottom:4px}
input{width:100%;background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:8px;color:#e6edf3;font-size:0.85rem;margin-bottom:12px}
input:focus{outline:none;border-color:#58a6ff}
.note{font-size:0.72rem;color:#6e7681;margin-top:-8px;margin-bottom:12px}
button{width:100%;padding:10px;background:#238636;color:#fff;border:none;border-radius:6px;font-size:0.9rem;font-weight:600;cursor:pointer}
button:hover{opacity:0.85}
</style>
</head>
<body>
<h1>&#127807; NPK Sensor Setup</h1>
<form action="/save" method="POST">

<div class="card">
<h2>WiFi</h2>
<label>SSID</label>
<input name="wssid" required maxlength="63" placeholder="Your WiFi name">
<label>Password</label>
<input name="wpass" type="password" maxlength="63" placeholder="Your WiFi password">
</div>

<div class="card">
<h2>MQTT Broker (Home Assistant)</h2>
<label>Broker IP / Hostname</label>
<input name="mhost" maxlength="63" placeholder="192.168.1.x  (leave blank to disable)">
<label>Port</label>
<input name="mport" type="number" value="1883" min="1" max="65535">
<label>Username (optional)</label>
<input name="muser" maxlength="31">
<label>Password (optional)</label>
<input name="mpass" type="password" maxlength="31">
</div>

<div class="card">
<h2>Web Dashboard Auth</h2>
<label>Username</label>
<input name="auser" value="admin" required maxlength="31">
<label>Password</label>
<input name="apass" type="password" value="admin" required maxlength="31">
</div>

<div class="card">
<h2>Device</h2>
<label>Device ID  (used as MQTT topic prefix)</label>
<input name="devid" value="soil_npk" required maxlength="31">
<p class="note">Letters, numbers and underscores only — e.g. soil_npk</p>
</div>

<button type="submit">Save &amp; Connect</button>
</form>
</body>
</html>
)rawliteral";

// ── Dashboard (STA mode, basic-auth protected) ───────────────
const char DASHBOARD_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Soil Sensor Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px;padding-bottom:12px;border-bottom:1px solid #21262d}
header h1{font-size:1.1rem;font-weight:600;color:#58a6ff}
.hdr-right{display:flex;align-items:center;gap:14px}
a.cfg{font-size:0.78rem;color:#58a6ff;text-decoration:none}
#status{display:flex;align-items:center;gap:6px;font-size:0.75rem;color:#8b949e}
#dot{width:8px;height:8px;border-radius:50%;background:#3fb950;box-shadow:0 0 6px #3fb950;transition:background 0.3s}
#dot.err{background:#f85149;box-shadow:0 0 6px #f85149}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(140px,1fr));gap:12px;margin-bottom:20px}
.card{background:#161b22;border:1px solid #21262d;border-radius:10px;padding:14px 12px;text-align:center;transition:border-color 0.3s}
.card.err{border-color:#f85149}
.card .label{font-size:0.7rem;color:#8b949e;text-transform:uppercase;letter-spacing:0.05em;margin-bottom:6px}
.card .val{font-size:1.6rem;font-weight:700;color:#58a6ff;line-height:1;transition:color 0.3s}
.card.err .val{color:#f85149;font-size:1rem;padding-top:6px}
.card .unit{font-size:0.65rem;color:#6e7681;margin-top:4px}
.card .icon{font-size:1.1rem;margin-bottom:4px}
section{background:#161b22;border:1px solid #21262d;border-radius:10px;padding:16px}
section h2{font-size:0.85rem;color:#8b949e;text-transform:uppercase;letter-spacing:0.05em;margin-bottom:14px}
.offset-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:10px;margin-bottom:14px}
.offset-item label{display:block;font-size:0.7rem;color:#8b949e;margin-bottom:4px}
.offset-item input{width:100%;background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:6px 8px;color:#e6edf3;font-size:0.85rem}
.offset-item input:focus{outline:none;border-color:#58a6ff}
.btn-row{display:flex;gap:10px}
button{padding:8px 20px;border:none;border-radius:6px;font-size:0.85rem;font-weight:600;cursor:pointer;transition:opacity 0.2s}
button:hover{opacity:0.85}
#btnApply{background:#238636;color:#fff}
#btnReset{background:#21262d;color:#e6edf3;border:1px solid #30363d}
#toast{position:fixed;bottom:20px;right:20px;background:#238636;color:#fff;padding:8px 16px;border-radius:8px;font-size:0.8rem;opacity:0;transition:opacity 0.4s;pointer-events:none}
</style>
</head>
<body>
<header>
  <h1>&#127807; Soil Sensor Dashboard</h1>
  <div class="hdr-right">
    <a class="cfg" href="/config">&#9881; Settings</a>
    <div id="status"><div id="dot"></div><span id="statusTxt">Connecting...</span></div>
  </div>
</header>

<div class="grid">
  <div class="card" id="cN"><div class="icon">🌿</div><div class="label">Nitrogen</div><div class="val" id="nVal">--</div><div class="unit">mg/kg</div></div>
  <div class="card" id="cP"><div class="icon">🔴</div><div class="label">Phosphorus</div><div class="val" id="pVal">--</div><div class="unit">mg/kg</div></div>
  <div class="card" id="cK"><div class="icon">🟡</div><div class="label">Potassium</div><div class="val" id="kVal">--</div><div class="unit">mg/kg</div></div>
  <div class="card" id="cPH"><div class="icon">⚗️</div><div class="label">pH</div><div class="val" id="phVal">--</div><div class="unit">&nbsp;</div></div>
  <div class="card" id="cEC"><div class="icon">⚡</div><div class="label">EC</div><div class="val" id="ecVal">--</div><div class="unit">µS/cm</div></div>
  <div class="card" id="cM"><div class="icon">💧</div><div class="label">Moisture</div><div class="val" id="mVal">--</div><div class="unit">%</div></div>
  <div class="card" id="cT"><div class="icon">🌡️</div><div class="label">Temperature</div><div class="val" id="tVal">--</div><div class="unit">°C</div></div>
</div>

<section>
  <h2>Calibration Offsets</h2>
  <form id="offsetForm">
    <div class="offset-grid">
      <div class="offset-item"><label>Nitrogen (N)</label><input type="number" step="0.1" name="n" id="in_n" value="0"></div>
      <div class="offset-item"><label>Phosphorus (P)</label><input type="number" step="0.1" name="p" id="in_p" value="0"></div>
      <div class="offset-item"><label>Potassium (K)</label><input type="number" step="0.1" name="k" id="in_k" value="0"></div>
      <div class="offset-item"><label>pH</label><input type="number" step="0.01" name="ph" id="in_ph" value="0"></div>
      <div class="offset-item"><label>EC</label><input type="number" step="0.1" name="ec" id="in_ec" value="0"></div>
      <div class="offset-item"><label>Moisture</label><input type="number" step="0.1" name="moist" id="in_moist" value="0"></div>
      <div class="offset-item"><label>Temperature</label><input type="number" step="0.1" name="temp" id="in_temp" value="0"></div>
    </div>
    <div class="btn-row">
      <button type="button" id="btnApply" onclick="sendOffsets()">Apply Offsets</button>
      <button type="button" id="btnReset" onclick="resetOffsets()">Reset</button>
    </div>
  </form>
</section>

<div id="toast">Offsets applied!</div>

<script>
function set(id,v,cardId){
  var el=document.getElementById(id);
  var card=document.getElementById(cardId);
  if(v==-1){el.innerText='ERR';card.classList.add('err');}
  else{el.innerText=v;card.classList.remove('err');}
}
async function fetchData(){
  try{
    var r=await fetch('/data');
    var j=await r.json();
    set('nVal',j.N,'cN');set('pVal',j.P,'cP');set('kVal',j.K,'cK');
    set('phVal',j.pH,'cPH');set('ecVal',j.EC,'cEC');
    set('mVal',j.Moist,'cM');set('tVal',j.Temp,'cT');
    document.getElementById('dot').className='';
    document.getElementById('statusTxt').innerText='Updated '+new Date().toLocaleTimeString();
  }catch(e){
    document.getElementById('dot').className='err';
    document.getElementById('statusTxt').innerText='No response';
  }
}
async function sendOffsets(){
  var data=new URLSearchParams(new FormData(document.getElementById('offsetForm')));
  await fetch('/offsets',{method:'POST',body:data});
  var t=document.getElementById('toast');
  t.style.opacity=1;setTimeout(function(){t.style.opacity=0;},2000);
}
function resetOffsets(){
  document.getElementById('offsetForm').querySelectorAll('input').forEach(function(i){i.value=0;});
  sendOffsets();
}
async function loadOffsets(){
  try{
    var r=await fetch('/offsets');
    var j=await r.json();
    document.getElementById('in_n').value=j.n;
    document.getElementById('in_p').value=j.p;
    document.getElementById('in_k').value=j.k;
    document.getElementById('in_ph').value=j.ph;
    document.getElementById('in_ec').value=j.ec;
    document.getElementById('in_moist').value=j.moist;
    document.getElementById('in_temp').value=j.temp;
  }catch(e){}
}
setInterval(fetchData,2000);
fetchData();
loadOffsets();
</script>
</body>
</html>
)rawliteral";

// ── Settings page (accessible from dashboard, basic-auth protected) ─
String settingsPage() {
    String p;
    p.reserve(3000);
    p += F("<!DOCTYPE html><html lang='en'><head>"
           "<meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width,initial-scale=1'>"
           "<title>Settings</title><style>"
           "*{box-sizing:border-box;margin:0;padding:0}"
           "body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:16px}"
           "h1{color:#58a6ff;font-size:1.1rem;margin-bottom:20px}"
           "a{color:#58a6ff;font-size:0.8rem;text-decoration:none}"
           "h2{color:#8b949e;font-size:0.78rem;text-transform:uppercase;letter-spacing:0.05em;margin:0 0 10px}"
           ".card{background:#161b22;border:1px solid #21262d;border-radius:10px;padding:16px;margin-bottom:14px}"
           "label{display:block;font-size:0.75rem;color:#8b949e;margin-bottom:4px}"
           "input{width:100%;background:#0d1117;border:1px solid #30363d;border-radius:6px;padding:8px;color:#e6edf3;font-size:0.85rem;margin-bottom:12px}"
           "input:focus{outline:none;border-color:#58a6ff}"
           ".note{font-size:0.72rem;color:#6e7681;margin-top:-8px;margin-bottom:12px}"
           ".btn-row{display:flex;gap:10px;flex-wrap:wrap}"
           "button{padding:9px 20px;border:none;border-radius:6px;font-size:0.85rem;font-weight:600;cursor:pointer}"
           "button:hover{opacity:0.85}"
           "#btnSave{background:#238636;color:#fff}"
           "#btnFactory{background:#da3633;color:#fff}"
           "</style></head><body>");
    p += F("<h1>&#9881; Settings &nbsp;<a href='/'>&#8592; Dashboard</a></h1>");
    p += F("<form action='/config' method='POST'>");

    // WiFi
    p += F("<div class='card'><h2>WiFi</h2>");
    p += F("<label>SSID</label><input name='wssid' required maxlength='63' value='");
    p += String(wifiSSID);
    p += F("'><label>Password</label>");
    p += F("<input name='wpass' type='password' maxlength='63' placeholder='(leave blank to keep current)'>");
    p += F("</div>");

    // MQTT
    p += F("<div class='card'><h2>MQTT Broker (Home Assistant)</h2>");
    p += F("<label>Broker IP / Hostname</label><input name='mhost' maxlength='63' value='");
    p += String(mqttHost);
    p += F("'><label>Port</label><input name='mport' type='number' min='1' max='65535' value='");
    p += String(mqttPort);
    p += F("'><label>Username (optional)</label><input name='muser' maxlength='31' value='");
    p += String(mqttUser);
    p += F("'><label>Password (optional)</label>");
    p += F("<input name='mpass' type='password' maxlength='31' placeholder='(leave blank to keep current)'>");
    p += F("<p class='note'>Leave host blank to disable MQTT.</p></div>");

    // Auth
    p += F("<div class='card'><h2>Web Dashboard Auth</h2>");
    p += F("<label>Username</label><input name='auser' required maxlength='31' value='");
    p += String(webUser);
    p += F("'><label>Password</label>");
    p += F("<input name='apass' type='password' required maxlength='31' placeholder='(leave blank to keep current)'>");
    p += F("</div>");

    // Device ID
    p += F("<div class='card'><h2>Device</h2>");
    p += F("<label>Device ID (MQTT topic prefix)</label>");
    p += F("<input name='devid' required maxlength='31' value='");
    p += String(deviceId);
    p += F("'><p class='note'>Changing this re-registers all sensors in Home Assistant.</p></div>");

    p += F("<div class='btn-row'>");
    p += F("<button type='submit' id='btnSave'>Save &amp; Reboot</button>");
    p += F("<button type='button' id='btnFactory' "
           "onclick=\"if(confirm('Erase ALL config and reboot?'))"
           "fetch('/reset',{method:'POST'})\">Factory Reset</button>");
    p += F("</div></form></body></html>");
    return p;
}

// =============================================================
//  Auth helper
// =============================================================
bool checkAuth() {
    if (!server.authenticate(webUser, webPass)) {
        server.requestAuthentication(BASIC_AUTH, "NPK Sensor", "Login required");
        return false;
    }
    return true;
}

// =============================================================
//  Route registrations
// =============================================================

void setupConfigRoutes() {
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", CONFIG_PAGE);
    });

    server.on("/save", HTTP_POST, []() {
        if (server.hasArg("wssid"))
            saveWifiConfig(server.arg("wssid").c_str(),
                           server.arg("wpass").c_str());

        saveMqttConfig(server.arg("mhost").c_str(),
                       server.hasArg("mport") ? (uint16_t)server.arg("mport").toInt() : 1883,
                       server.arg("muser").c_str(),
                       server.arg("mpass").c_str());

        saveAuthConfig(server.arg("auser").c_str(),
                       server.arg("apass").c_str(),
                       server.arg("devid").c_str());

        server.send(200, "text/html",
            "<html><body style='background:#0d1117;color:#e6edf3;"
            "font-family:sans-serif;padding:24px'>"
            "<h2 style='color:#3fb950'>&#10003; Saved! Connecting...</h2>"
            "<p style='color:#8b949e;margin-top:10px'>The device will reboot and join your WiFi.</p>"
            "</body></html>");
        delay(1500);
        ESP.restart();
    });
}

void setupDashRoutes() {
    // Dashboard
    server.on("/", HTTP_GET, []() {
        if (!checkAuth()) return;
        server.send_P(200, "text/html", DASHBOARD_PAGE);
    });

    // Live sensor data (JSON)
    server.on("/data", HTTP_GET, []() {
        if (!checkAuth()) return;
        char json[200];
        snprintf(json, sizeof(json),
            "{\"N\":%.1f,\"P\":%.1f,\"K\":%.1f,"
            "\"pH\":%.2f,\"EC\":%.0f,"
            "\"Moist\":%.1f,\"Temp\":%.1f}",
            N, P, K, pH, EC, Moist, Temp);
        server.send(200, "application/json", json);
    });

    // Offsets GET
    server.on("/offsets", HTTP_GET, []() {
        if (!checkAuth()) return;
        char json[200];
        snprintf(json, sizeof(json),
            "{\"n\":%.2f,\"p\":%.2f,\"k\":%.2f,"
            "\"ph\":%.3f,\"ec\":%.2f,"
            "\"moist\":%.2f,\"temp\":%.2f}",
            offset_N, offset_P, offset_K,
            offset_pH, offset_EC, offset_Moist, offset_Temp);
        server.send(200, "application/json", json);
    });

    // Offsets POST
    server.on("/offsets", HTTP_POST, []() {
        if (!checkAuth()) return;
        if (server.hasArg("n"))     offset_N     = server.arg("n").toFloat();
        if (server.hasArg("p"))     offset_P     = server.arg("p").toFloat();
        if (server.hasArg("k"))     offset_K     = server.arg("k").toFloat();
        if (server.hasArg("ph"))    offset_pH    = server.arg("ph").toFloat();
        if (server.hasArg("ec"))    offset_EC    = server.arg("ec").toFloat();
        if (server.hasArg("moist")) offset_Moist = server.arg("moist").toFloat();
        if (server.hasArg("temp"))  offset_Temp  = server.arg("temp").toFloat();
        saveOffsets();
        server.send(200, "text/plain", "OK");
    });

    // Settings page GET
    server.on("/config", HTTP_GET, []() {
        if (!checkAuth()) return;
        server.send(200, "text/html", settingsPage());
    });

    // Settings page POST (save + reboot)
    server.on("/config", HTTP_POST, []() {
        if (!checkAuth()) return;
        saveWifiConfig(server.arg("wssid").c_str(),
                       server.arg("wpass").c_str());
        saveMqttConfig(server.arg("mhost").c_str(),
                       server.hasArg("mport") ? (uint16_t)server.arg("mport").toInt() : 1883,
                       server.arg("muser").c_str(),
                       server.arg("mpass").c_str());
        saveAuthConfig(server.arg("auser").c_str(),
                       server.arg("apass").c_str(),
                       server.arg("devid").c_str());

        server.send(200, "text/html",
            "<html><body style='background:#0d1117;color:#e6edf3;"
            "font-family:sans-serif;padding:24px'>"
            "<h2 style='color:#3fb950'>&#10003; Settings saved! Rebooting...</h2>"
            "</body></html>");
        delay(1500);
        ESP.restart();
    });

    // Factory reset
    server.on("/reset", HTTP_POST, []() {
        if (!checkAuth()) return;
        clearAllConfig();
        server.send(200, "text/plain", "Factory reset done. Rebooting...");
        delay(1500);
        ESP.restart();
    });
}

// =============================================================
//  Setup
// =============================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== ESP32 NPK Sensor v2 ===");

    // Check BOOT button (held LOW = force config portal)
    pinMode(BOOT_BTN, INPUT_PULLUP);
    delay(100);
    bool forceConfig = (digitalRead(BOOT_BTN) == LOW);
    if (forceConfig) Serial.println("BOOT held → forcing config portal");

    // RS485 init
    pinMode(DE_RE_PIN, OUTPUT);
    digitalWrite(DE_RE_PIN, LOW);
    Serial2.begin(4800, SERIAL_8N1, RX2_PIN, TX2_PIN);
    node.begin(1, Serial2);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    // Load NVS
    loadConfig();

    // ── Config portal mode ───────────────────────────────────
    if (forceConfig || strlen(wifiSSID) == 0) {
        configMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("NPK-Config");
        Serial.print("Config portal → connect to 'NPK-Config' (open), open ");
        Serial.println(WiFi.softAPIP());
        setupConfigRoutes();
        server.begin();
        return;
    }

    // ── Normal STA mode ──────────────────────────────────────
    Serial.printf("Connecting to WiFi: %s\n", wifiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPass);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
        delay(500); Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() != WL_CONNECTED) {
        // Fallback: open config portal
        Serial.println("WiFi failed → config portal");
        configMode = true;
        WiFi.mode(WIFI_AP);
        WiFi.softAP("NPK-Config");
        Serial.print("Config portal IP: ");
        Serial.println(WiFi.softAPIP());
        setupConfigRoutes();
        server.begin();
        return;
    }

    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());

    // mDNS — sanitise deviceId: replace '_' with '-' (mDNS disallows underscores)
    char mdnsName[32];
    strlcpy(mdnsName, deviceId, sizeof(mdnsName));
    for (char* c = mdnsName; *c; c++) if (*c == '_') *c = '-';
    if (MDNS.begin(mdnsName)) {
        MDNS.addService("http", "tcp", 80);
        Serial.printf("mDNS started → http://%s.local\n", mdnsName);
    } else {
        Serial.println("mDNS failed (non-fatal)");
    }

    // MQTT
    if (strlen(mqttHost) > 0) {
        mqtt.setServer(mqttHost, mqttPort);
        mqtt.setBufferSize(512);
        mqttReconnect();
    }

    setupDashRoutes();
    server.begin();
    Serial.println("HTTP server started");
}

// =============================================================
//  Loop
// =============================================================
void loop() {
    server.handleClient();

    if (configMode) return;   // no sensor reads or MQTT in config portal mode

    // WiFi watchdog
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (millis() - lastReconnect > 10000) {
            lastReconnect = millis();
            Serial.println("WiFi lost, reconnecting...");
            WiFi.reconnect();
        }
        return;
    }

    // MQTT keepalive
    if (strlen(mqttHost) > 0) {
        if (!mqtt.connected()) {
            if (millis() - lastMqttAttempt > 5000) {
                lastMqttAttempt = millis();
                mqttReconnect();
            }
        }
        mqtt.loop();
    }

    // Sensor read (every READ_INTERVAL ms)
    if (millis() - lastRead >= READ_INTERVAL) {
        lastRead = millis();
        uint8_t result = node.readHoldingRegisters(0x0000, 7);
        if (result == node.ku8MBSuccess) {
            Moist = node.getResponseBuffer(0) / 10.0f + offset_Moist;
            Temp  = node.getResponseBuffer(1) / 10.0f + offset_Temp;
            EC    = node.getResponseBuffer(2)          + offset_EC;
            pH    = node.getResponseBuffer(3) / 10.0f + offset_pH;
            N     = node.getResponseBuffer(4) / 10.0f + offset_N;
            P     = node.getResponseBuffer(5) / 10.0f + offset_P;
            K     = node.getResponseBuffer(6) / 10.0f + offset_K;
        } else {
            N = P = K = pH = EC = Moist = Temp = -1;
            Serial.printf("Modbus error: 0x%02X\n", result);
        }
    }

    // MQTT publish (every MQTT_INTERVAL ms)
    if (strlen(mqttHost) > 0 && mqtt.connected()) {
        if (millis() - lastMqttPublish >= MQTT_INTERVAL) {
            lastMqttPublish = millis();
            mqttPublishState();
        }
    }
}
