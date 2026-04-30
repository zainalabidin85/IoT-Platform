/**************************************************************
 * NPKv2Node — ESP32 Dev
 *  Sensor: CWT-SOIL-NPKPHCTH-S via MAX485 (RS485 / Modbus RTU)
 *
 * AP Captive Portal: open (provisioning)
 * STA Dashboard: Basic Auth  user=admin  pass=npknode
 *
 * Pins:
 *  - RS485 RX:    GPIO16
 *  - RS485 TX:    GPIO17
 *  - RS485 DE/RE: GPIO4
 *  - BOOT BTN:    GPIO0  (hold at power-up → force AP mode)
 **************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "mqtt_client.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "esp_wifi.h"
#include <ModbusMaster.h>

// ── TLS cert (ISRG Root X1 — for api-iot.unitani.com & WSS MQTT) ──
static const char ISRG_ROOT_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// ── Provision HTML (embedded — served in STA mode) ─────────────
static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NPK Node - Provision</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px}
.card{max-width:520px;padding:16px;border:1px solid #ddd;border-radius:12px}
input{width:100%;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:10px;box-sizing:border-box}
button{width:100%;padding:12px;border:0;border-radius:10px;background:#2a8cff;color:white;font-weight:700;cursor:pointer}
button:disabled{background:#aaa}
small{color:#666}
#msg{margin-top:12px;padding:10px;border-radius:8px;display:none}
.ok{background:#d4edda;color:#155724}
.err{background:#f8d7da;color:#721c24}
</style></head><body>
<div class="card">
<h2>Connect to Platform</h2>
<p><small>Enter your topic and API key from iot.unitani.com. MQTT credentials will be configured automatically.</small></p>
<label>Topic</label>
<input id="topic" placeholder="e.g. myfield-npk1" required>
<label>API Key</label>
<input id="apikey" type="password" placeholder="Paste API key from dashboard" required>
<button id="btn" onclick="doProvision()">Connect</button>
<div id="msg"></div>
<p style="margin-top:12px"><a href="/">Home</a></p>
</div>
<script>
async function doProvision() {
  const topic = document.getElementById('topic').value.trim();
  const apikey = document.getElementById('apikey').value.trim();
  const btn = document.getElementById('btn');
  if (!topic || !apikey) { showMsg('Topic and API key are required.', false); return; }
  btn.disabled = true; btn.textContent = 'Connecting...';
  document.getElementById('msg').style.display = 'none';
  try {
    const fd = new FormData();
    fd.append('topic', topic);
    fd.append('api_key', apikey);
    const r = await fetch('/api/provision', { method: 'POST', body: fd });
    const j = await r.json();
    if (j.ok) {
      showMsg('Provisioned! MQTT is now configured.', true);
      btn.textContent = 'Done';
    } else {
      showMsg('Error: ' + (j.error || 'unknown'), false);
      btn.disabled = false; btn.textContent = 'Connect';
    }
  } catch(e) {
    showMsg('Request failed: ' + e, false);
    btn.disabled = false; btn.textContent = 'Connect';
  }
}
function showMsg(text, ok) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.className = ok ? 'ok' : 'err';
  el.style.display = 'block';
}
</script>
</body></html>
)HTML";

// ── Dashboard HTML (embedded — served in STA mode) ─────────────
static const char DASHBOARD_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NPK Sensor Dashboard</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:#0d1117;color:#e6edf3;min-height:100vh;padding:16px}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:20px;padding-bottom:12px;border-bottom:1px solid #21262d}
header h1{font-size:1.1rem;font-weight:600;color:#58a6ff}
.hdr-right{display:flex;align-items:center;gap:14px}
a.lnk{font-size:0.78rem;color:#58a6ff;text-decoration:none}
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
  <h1>&#127807; NPK Sensor Dashboard</h1>
  <div class="hdr-right">
    <a class="lnk" href="/provision">&#128279; Provision</a>
    <a class="lnk" href="/settings">&#9881; Settings</a>
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
<div id="toast">Offsets saved!</div>
<script>
function setVal(id, v, cardId) {
  var el = document.getElementById(id), card = document.getElementById(cardId);
  if (v === null || v === -1) { el.innerText = 'ERR'; card.classList.add('err'); }
  else { el.innerText = v; card.classList.remove('err'); }
}
async function fetchData() {
  try {
    var j = await (await fetch('/api/data')).json();
    setVal('nVal', j.N, 'cN'); setVal('pVal', j.P, 'cP'); setVal('kVal', j.K, 'cK');
    setVal('phVal', j.pH, 'cPH'); setVal('ecVal', j.EC, 'cEC');
    setVal('mVal', j.Moist, 'cM'); setVal('tVal', j.Temp, 'cT');
    document.getElementById('dot').className = '';
    document.getElementById('statusTxt').innerText = 'Updated ' + new Date().toLocaleTimeString();
  } catch(e) {
    document.getElementById('dot').className = 'err';
    document.getElementById('statusTxt').innerText = 'No response';
  }
}
async function sendOffsets() {
  await fetch('/api/offsets', { method: 'POST', body: new URLSearchParams(new FormData(document.getElementById('offsetForm'))) });
  var t = document.getElementById('toast'); t.style.opacity = 1; setTimeout(function(){ t.style.opacity = 0; }, 2000);
}
function resetOffsets() {
  document.getElementById('offsetForm').querySelectorAll('input').forEach(function(i){ i.value = 0; });
  sendOffsets();
}
async function loadOffsets() {
  try {
    var j = await (await fetch('/api/offsets')).json();
    document.getElementById('in_n').value = j.n;
    document.getElementById('in_p').value = j.p;
    document.getElementById('in_k').value = j.k;
    document.getElementById('in_ph').value = j.ph;
    document.getElementById('in_ec').value = j.ec;
    document.getElementById('in_moist').value = j.moist;
    document.getElementById('in_temp').value = j.temp;
  } catch(e) {}
}
setInterval(fetchData, 2000);
fetchData();
loadOffsets();
</script>
</body>
</html>
)HTML";

// ── Pins ───────────────────────────────────────────────────────
#define RX2_PIN    16
#define TX2_PIN    17
#define DE_RE_PIN   4
#define BOOT_BTN    0

// ── FS / DNS ───────────────────────────────────────────────────
static const char*  FS_ROOT  = "/www";
static const byte   DNS_PORT = 53;

// ── Constants ──────────────────────────────────────────────────
static const char* FW_VERSION       = "1.0.0";
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "npknode";

static const bool  BASIC_AUTH_ON = true;
static const char* BASIC_USER   = "admin";
static const char* BASIC_PASS   = "npknode";

static const uint32_t READ_INTERVAL    = 2000;
static const uint32_t PUBLISH_INTERVAL = 30000;

// ── Web / DNS ──────────────────────────────────────────────────
AsyncWebServer server(80);
DNSServer      dns;

// ── MQTT ───────────────────────────────────────────────────────
static esp_mqtt_client_handle_t mqttHandle    = nullptr;
static volatile bool            mqttConnected = false;

// ── NVS ────────────────────────────────────────────────────────
Preferences prefs;

// ── Device IDs ─────────────────────────────────────────────────
String deviceId, shortId, mdnsHost, mdnsFqdn;
static String _otaTopic;

// ── WiFi config ────────────────────────────────────────────────
struct WifiCfg { String ssid, pass; } wifiCfg;

// ── MQTT config ────────────────────────────────────────────────
struct MqttCfg {
  bool   enabled   = false;
  String uri, user, pass, cmdTopic;
} mqttCfg;

// ── MQTT topics ─────────────────────────────────────────────────
String topicTelemetry, topicStatus;

// ── Sensor readings ────────────────────────────────────────────
struct Sensor {
  float N=-1, P=-1, K=-1, pH=-1, EC=-1, Moist=-1, Temp=-1;
  bool  valid = false;
} sens;

// ── Calibration offsets ────────────────────────────────────────
struct Offsets {
  float N=0, P=0, K=0, pH=0, EC=0, Moist=0, Temp=0;
} offs;

// ── Timing ─────────────────────────────────────────────────────
static uint32_t lastRead    = 0;
static uint32_t lastPublish = 0;

// ── Modbus ─────────────────────────────────────────────────────
ModbusMaster modbusNode;
static void preTransmission()  { digitalWrite(DE_RE_PIN, HIGH); }
static void postTransmission() { digitalWrite(DE_RE_PIN, LOW);  }

// ── Mode ───────────────────────────────────────────────────────
enum Mode { MODE_AP, MODE_STA };
static Mode modeNow = MODE_AP;

// =============================================================
//  Helpers
// =============================================================
static String macToDeviceId() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char buf[32]; snprintf(buf, sizeof(buf), "esp32-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}
static String macSuffix6() {
  uint8_t mac[6]; WiFi.macAddress(mac);
  char buf[8]; snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}
static void applyTopics() {
  topicTelemetry = mqttCfg.cmdTopic + "/telemetry";
  topicStatus    = mqttCfg.cmdTopic + "/status";
}

// =============================================================
//  NVS
// =============================================================
static void loadWifiCfg() {
  prefs.begin("wifi", true);
  wifiCfg.ssid = prefs.getString("ssid", "");
  wifiCfg.pass = prefs.getString("pass", "");
  prefs.end();
}
static void saveWifiCfg() {
  prefs.begin("wifi", false);
  prefs.putString("ssid", wifiCfg.ssid);
  prefs.putString("pass", wifiCfg.pass);
  prefs.end();
}
static void loadMqttCfg() {
  prefs.begin("mqtt", true);
  mqttCfg.enabled  = prefs.getBool("en", false);
  mqttCfg.uri      = prefs.getString("uri",  "");
  mqttCfg.user     = prefs.getString("user", "");
  mqttCfg.pass     = prefs.getString("pass", "");
  mqttCfg.cmdTopic = prefs.getString("cmd",  "");
  prefs.end();
  applyTopics();
}
static void saveMqttCfg() {
  prefs.begin("mqtt", false);
  prefs.putBool("en",     mqttCfg.enabled);
  prefs.putString("uri",  mqttCfg.uri);
  prefs.putString("user", mqttCfg.user);
  prefs.putString("pass", mqttCfg.pass);
  prefs.putString("cmd",  mqttCfg.cmdTopic);
  prefs.end();
}
static void loadOffsets() {
  prefs.begin("offsets", true);
  offs.N     = prefs.getFloat("n",     0);
  offs.P     = prefs.getFloat("p",     0);
  offs.K     = prefs.getFloat("k",     0);
  offs.pH    = prefs.getFloat("ph",    0);
  offs.EC    = prefs.getFloat("ec",    0);
  offs.Moist = prefs.getFloat("moist", 0);
  offs.Temp  = prefs.getFloat("temp",  0);
  prefs.end();
}
static void saveOffsets() {
  prefs.begin("offsets", false);
  prefs.putFloat("n",     offs.N);
  prefs.putFloat("p",     offs.P);
  prefs.putFloat("k",     offs.K);
  prefs.putFloat("ph",    offs.pH);
  prefs.putFloat("ec",    offs.EC);
  prefs.putFloat("moist", offs.Moist);
  prefs.putFloat("temp",  offs.Temp);
  prefs.end();
}

// =============================================================
//  OTA
// =============================================================
static void performOTA(const String& url) {
  Serial.printf("[OTA] Starting from: %s\n", url.c_str());
  WiFiClient httpClient;
  httpUpdate.rebootOnUpdate(true);
  auto ret = httpUpdate.update(httpClient, url);
  if (ret == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
}

// =============================================================
//  Modbus / Sensor
// =============================================================
static void readSensor() {
  uint8_t result = modbusNode.readHoldingRegisters(0x0000, 7);
  if (result == modbusNode.ku8MBSuccess) {
    sens.Moist = modbusNode.getResponseBuffer(0) / 10.0f + offs.Moist;
    sens.Temp  = modbusNode.getResponseBuffer(1) / 10.0f + offs.Temp;
    sens.EC    = modbusNode.getResponseBuffer(2)          + offs.EC;
    sens.pH    = modbusNode.getResponseBuffer(3) / 10.0f + offs.pH;
    sens.N     = modbusNode.getResponseBuffer(4) / 10.0f + offs.N;
    sens.P     = modbusNode.getResponseBuffer(5) / 10.0f + offs.P;
    sens.K     = modbusNode.getResponseBuffer(6) / 10.0f + offs.K;
    sens.valid = true;
  } else {
    sens.N = sens.P = sens.K = sens.pH = sens.EC = sens.Moist = sens.Temp = -1;
    sens.valid = false;
    Serial.printf("[Modbus] error: 0x%02X\n", result);
  }
}

// =============================================================
//  MQTT
// =============================================================
static void publishTelemetry() {
  if (!mqttConnected || !mqttCfg.cmdTopic.length()) return;
  StaticJsonDocument<256> doc;
  doc["N"]     = sens.N;
  doc["P"]     = sens.P;
  doc["K"]     = sens.K;
  doc["pH"]    = sens.pH;
  doc["EC"]    = sens.EC;
  doc["Moist"] = sens.Moist;
  doc["Temp"]  = sens.Temp;
  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, topicTelemetry.c_str(), buf, 0, 0, 0);
  Serial.printf("[MQTT] telemetry → %s\n", topicTelemetry.c_str());
}

static void mqttEventHandler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("[MQTT] Connected.");
      _otaTopic = mqttCfg.cmdTopic + "/ota/command";
      esp_mqtt_client_subscribe(mqttHandle, _otaTopic.c_str(), 1);
      esp_mqtt_client_publish(mqttHandle, topicStatus.c_str(), "online", 0, 1, 1);
      publishTelemetry();
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      Serial.println("[MQTT] Disconnected.");
      break;
    case MQTT_EVENT_DATA:
      if (ev->topic && ev->data) {
        String topic(ev->topic, ev->topic_len);
        String msg(ev->data, ev->data_len);
        if (topic == _otaTopic) {
          DynamicJsonDocument doc(256);
          if (deserializeJson(doc, msg) == DeserializationError::Ok) {
            const char* ver = doc["version"];
            const char* url = doc["url"];
            if (url && ver && String(ver) != FW_VERSION) {
              Serial.printf("[OTA] New fw %s\n", ver);
              performOTA(url);
            }
          }
        }
      }
      break;
    default: break;
  }
}

static void mqttStart() {
  if (!mqttCfg.enabled || !mqttCfg.uri.length() || !mqttCfg.cmdTopic.length()) return;
  if (mqttHandle) {
    esp_mqtt_client_stop(mqttHandle);
    esp_mqtt_client_destroy(mqttHandle);
    mqttHandle = nullptr;
    mqttConnected = false;
  }
  const String clientId = mdnsHost + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  esp_mqtt_client_config_t cfg = {};
  cfg.uri                  = mqttCfg.uri.c_str();
  cfg.client_id            = clientId.c_str();
  cfg.username             = mqttCfg.user.c_str();
  cfg.password             = mqttCfg.pass.c_str();
  cfg.lwt_topic            = topicStatus.c_str();
  cfg.lwt_msg              = "offline";
  cfg.lwt_msg_len          = 7;
  cfg.lwt_qos              = 1;
  cfg.lwt_retain           = 1;
  cfg.reconnect_timeout_ms = 5000;
  cfg.cert_pem             = ISRG_ROOT_X1;
  Serial.printf("[MQTT] Starting — uri=%s\n", mqttCfg.uri.c_str());
  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

// =============================================================
//  WiFi
// =============================================================
static bool connectSTA(uint32_t timeoutMs = 20000) {
  if (!wifiCfg.ssid.length()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(mdnsHost.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());
  Serial.printf("[WiFi] Connecting to %s...\n", wifiCfg.ssid.c_str());
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected! IP=%s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    delay(250);
  }
  Serial.println("[WiFi] Timeout.");
  return false;
}

static void startAPPortal() {
  WiFi.disconnect(true, true);
  delay(200);
  const String apSsid = "NPKNode-" + shortId;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), nullptr);
  delay(200);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(DNS_PORT, "*", ip);
  Serial.println("[AP] SSID: " + apSsid + "  IP: " + ip.toString());
}

static void startMDNS() {
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] http://" + mdnsFqdn + "/");
  }
}

// =============================================================
//  Platform provisioning
// =============================================================
static void doProvision(const String& topic, const String& apiKey,
                        AsyncWebServerRequest* req) {
  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  String url = String(PLATFORM_API_URL) + "/api/provision";
  http.begin(tls, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", apiKey);

  StaticJsonDocument<256> body;
  body["base_topic"] = topic;
  body["node_type"]  = NODE_TYPE;
  String bodyStr;
  serializeJson(body, bodyStr);

  int code = http.POST(bodyStr);
  if (code == 200 || code == 201) {
    String resp = http.getString();
    http.end();
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      JsonObject cfg = doc["mqtt_config"];
      if (cfg) {
        mqttCfg.enabled  = true;
        mqttCfg.uri      = cfg["uri"]   | "";
        mqttCfg.user     = cfg["user"]  | "";
        mqttCfg.pass     = cfg["pass"]  | "";
        mqttCfg.cmdTopic = cfg["topic"] | topic.c_str();
        saveMqttCfg();
        applyTopics();
        mqttStart();

        prefs.begin("prov", false);
        prefs.putBool("done", true);
        prefs.putString("apikey", apiKey);
        prefs.end();

        req->send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    req->send(502, "application/json", "{\"ok\":false,\"error\":\"bad_response\"}");
    return;
  }

  String errBody = http.getString();
  http.end();
  Serial.printf("[PROV] HTTP %d: %s\n", code, errBody.c_str());
  DynamicJsonDocument errDoc(512);
  String errMsg = "http_" + String(code);
  if (deserializeJson(errDoc, errBody) == DeserializationError::Ok)
    if (errDoc["error"].is<const char*>()) errMsg = errDoc["error"].as<String>();
  String out = "{\"ok\":false,\"error\":\"" + errMsg + "\"}";
  req->send(code == 409 ? 409 : 502, "application/json", out);
}

// =============================================================
//  Auth helpers
// =============================================================
static inline bool authOK(AsyncWebServerRequest* r) {
  if (!BASIC_AUTH_ON) return true;
  return r->authenticate(BASIC_USER, BASIC_PASS);
}
static inline bool requireAuth(AsyncWebServerRequest* r) {
  if (authOK(r)) return true;
  r->requestAuthentication();
  return false;
}

// =============================================================
//  AP routes
// =============================================================
static void setupRoutes_AP() {
  // Captive portal probes
  const auto redir = [](AsyncWebServerRequest* r){ r->redirect("/"); };
  server.on("/connecttest.txt",  HTTP_ANY, redir);
  server.on("/generate_204",     HTTP_ANY, redir);
  server.on("/hotspot-detect.html", HTTP_ANY, redir);
  server.on("/ncsi.txt",         HTTP_ANY, redir);
  server.on("/fwlink",           HTTP_ANY, redir);
  server.on("/redirect",         HTTP_ANY, redir);
  server.on("/canonical.html",   HTTP_ANY, redir);
  server.on("/success.txt",      HTTP_ANY, redir);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });

  // WiFi scan
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* r){
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i)
           + ",\"encryption\":\"" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURE") + "\"}";
    }
    json += "]}";
    r->send(200, "application/json", json);
    WiFi.scanDelete();
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r){
    String json = "{\"ok\":true,\"mdns\":\"" + mdnsFqdn + "\"}";
    r->send(200, "application/json", json);
  });

  // Save WiFi credentials and reboot
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest* r){
    if (!r->hasParam("ssid", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"ssid_required\"}");
      return;
    }
    wifiCfg.ssid = r->getParam("ssid", true)->value();
    wifiCfg.pass = r->hasParam("pass", true) ? r->getParam("pass", true)->value() : "";
    saveWifiCfg();
    r->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    delay(500);
    ESP.restart();
  });

  server.onNotFound([](AsyncWebServerRequest* r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });

  server.serveStatic("/", LittleFS, FS_ROOT);
  server.begin();
  Serial.println("[AP] Web server started (open).");
}

// =============================================================
//  STA routes
// =============================================================
static void setupRoutes_STA() {
  // Dashboard
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    r->send_P(200, "text/html", DASHBOARD_HTML);
  });

  // Provision page
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    r->send_P(200, "text/html", PROVISION_HTML);
  });

  // Provision action
  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    if (!r->hasParam("topic", true) || !r->hasParam("api_key", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_params\"}");
      return;
    }
    String topic  = r->getParam("topic",   true)->value(); topic.trim();
    String apiKey = r->getParam("api_key", true)->value(); apiKey.trim();
    if (!topic.length() || !apiKey.length()) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"empty_params\"}");
      return;
    }
    doProvision(topic, apiKey, r);
  });

  // Status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    StaticJsonDocument<512> d;
    d["ok"]             = true;
    d["ip"]             = WiFi.localIP().toString();
    d["mdns"]           = mdnsFqdn;
    d["rssi"]           = WiFi.RSSI();
    d["mqtt_enabled"]   = mqttCfg.enabled;
    d["mqtt_connected"] = mqttConnected;
    d["mqtt_uri"]       = mqttCfg.uri;
    d["cmd_topic"]      = mqttCfg.cmdTopic;
    d["sensor_valid"]   = sens.valid;
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Live sensor data
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    StaticJsonDocument<256> d;
    d["N"]     = sens.N;
    d["P"]     = sens.P;
    d["K"]     = sens.K;
    d["pH"]    = sens.pH;
    d["EC"]    = sens.EC;
    d["Moist"] = sens.Moist;
    d["Temp"]  = sens.Temp;
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Offsets GET
  server.on("/api/offsets", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    StaticJsonDocument<256> d;
    d["n"] = offs.N; d["p"] = offs.P; d["k"] = offs.K;
    d["ph"] = offs.pH; d["ec"] = offs.EC;
    d["moist"] = offs.Moist; d["temp"] = offs.Temp;
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Offsets POST
  server.on("/api/offsets", HTTP_POST, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    if (r->hasParam("n",     true)) offs.N     = r->getParam("n",     true)->value().toFloat();
    if (r->hasParam("p",     true)) offs.P     = r->getParam("p",     true)->value().toFloat();
    if (r->hasParam("k",     true)) offs.K     = r->getParam("k",     true)->value().toFloat();
    if (r->hasParam("ph",    true)) offs.pH    = r->getParam("ph",    true)->value().toFloat();
    if (r->hasParam("ec",    true)) offs.EC    = r->getParam("ec",    true)->value().toFloat();
    if (r->hasParam("moist", true)) offs.Moist = r->getParam("moist", true)->value().toFloat();
    if (r->hasParam("temp",  true)) offs.Temp  = r->getParam("temp",  true)->value().toFloat();
    saveOffsets();
    r->send(200, "text/plain", "OK");
  });

  // Settings page (redirect to provision for MQTT, shows device info)
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    String html = F("<!doctype html><html><head>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Settings</title>"
      "<style>body{font-family:system-ui,Arial;background:#0d1117;color:#e6edf3;padding:24px}"
      "h1{color:#58a6ff;margin-bottom:20px}.card{background:#161b22;border:1px solid #21262d;"
      "border-radius:10px;padding:16px;margin-bottom:14px;max-width:520px}"
      "label{font-size:0.8rem;color:#8b949e}p{margin:4px 0 12px;word-break:break-all}"
      "a.btn{display:inline-block;padding:10px 20px;background:#238636;color:#fff;"
      "text-decoration:none;border-radius:6px;font-weight:600;margin-top:8px}"
      "a.danger{background:#da3633}</style></head><body>"
      "<h1>&#9881; Settings</h1>"
      "<div class='card'><label>Device ID</label><p>");
    html += deviceId;
    html += F("</p><label>mDNS</label><p>");
    html += mdnsFqdn;
    html += F("</p><label>MQTT URI</label><p>");
    html += (mqttCfg.uri.length() ? mqttCfg.uri : "(not provisioned)");
    html += F("</p><label>Topic</label><p>");
    html += (mqttCfg.cmdTopic.length() ? mqttCfg.cmdTopic : "(not provisioned)");
    html += F("</p></div>"
      "<a class='btn' href='/provision'>&#128279; Platform Provision</a>&nbsp;"
      "<a class='btn danger' href='/' onclick=\""
        "if(confirm('Factory reset all config?'))"
        "fetch('/api/reset',{method:'POST'}).then(()=>setTimeout(()=>location.href='/',3000));"
        "return false;\">&#128465; Factory Reset</a>"
      "</body></html>");
    r->send(200, "text/html", html);
  });

  // Factory reset
  server.on("/api/reset", HTTP_POST, [](AsyncWebServerRequest* r){
    if (!requireAuth(r)) return;
    const char* ns[] = {"wifi", "mqtt", "offsets", "prov"};
    for (auto n : ns) { prefs.begin(n, false); prefs.clear(); prefs.end(); }
    r->send(200, "text/plain", "Reset done. Rebooting...");
    delay(1000);
    ESP.restart();
  });

  server.begin();
  Serial.println("[STA] Web server started (Basic Auth ON).");
}

// =============================================================
//  setup / loop
// =============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("=== NPKv2Node boot ===");

  // Check BOOT button (hold → force AP)
  pinMode(BOOT_BTN, INPUT_PULLUP);
  delay(100);
  bool forceAP = (digitalRead(BOOT_BTN) == LOW);
  if (forceAP) Serial.println("[BOOT] BOOT held → forcing AP mode");

  // RS485
  pinMode(DE_RE_PIN, OUTPUT);
  digitalWrite(DE_RE_PIN, LOW);
  Serial2.begin(4800, SERIAL_8N1, RX2_PIN, TX2_PIN);
  modbusNode.begin(1, Serial2);
  modbusNode.preTransmission(preTransmission);
  modbusNode.postTransmission(postTransmission);

  if (!LittleFS.begin(true))
    Serial.println("[FS] LittleFS mount failed.");
  else
    Serial.println("[FS] LittleFS mounted.");

  deviceId = macToDeviceId();
  shortId  = macSuffix6();
  mdnsHost = "npknode-" + shortId;
  mdnsFqdn = mdnsHost + ".local";

  loadWifiCfg();
  loadMqttCfg();
  loadOffsets();

  Serial.println("[ID] " + deviceId + "  mDNS: " + mdnsFqdn);

  if (!forceAP && connectSTA(20000)) {
    modeNow = MODE_STA;
    startMDNS();
    setupRoutes_STA();
    mqttStart();
  } else {
    modeNow = MODE_AP;
    startAPPortal();
    setupRoutes_AP();
  }
}

void loop() {
  if (modeNow == MODE_AP) {
    dns.processNextRequest();
    delay(10);
    return;
  }

  uint32_t now = millis();

  // Read sensor every READ_INTERVAL
  if (now - lastRead >= READ_INTERVAL) {
    lastRead = now;
    readSensor();
  }

  // Publish telemetry every PUBLISH_INTERVAL
  if (mqttConnected && now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    publishTelemetry();
  }

  delay(10);
}
