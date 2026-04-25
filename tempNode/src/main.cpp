/**************************************************************
 * Temp-Node (alphaCat-style skeleton)
 * - Sensor: DHT22 (temp + humidity) OR DS18B20 (temp only)
 *   Select with SENSOR_DHT22 or SENSOR_DS18B20 define below
 * - 3 buttons: LIGHT/MODE, UP, DOWN/ENTER
 * - LCD 20x4 I2C (0x27)
 * - Captive-portal AP fallback for Wi-Fi provisioning
 * - LittleFS serves /www UI files + file manager endpoints
 * - Always-available JSON APIs: /api/status, /api/temp
 * - Platform provisioning via /provision (topic + API key only)
 * - MQTT credentials auto-fetched from platform on provision
 * - OTA firmware update via MQTT ota/command topic
 **************************************************************/

// -------------------- Core --------------------
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <LittleFS.h>

// -------------------- Web --------------------
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// -------------------- LCD --------------------
#include <LiquidCrystal_I2C.h>

// -------------------- MQTT (optional) --------------------
#include "mqtt_client.h"

// -------------------- Platform provisioning --------------------
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

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

/**************************************************************
 *                  SENSOR SELECTION
 *  Uncomment exactly ONE of these:
 **************************************************************/
// #define SENSOR_DHT22    1   // DHT22 — temperature + humidity
#define SENSOR_DS18B20  1   // DS18B20 — temperature only (1-Wire)

#if defined(SENSOR_DHT22)
  #include <DHT.h>
#elif defined(SENSOR_DS18B20)
  #include <OneWire.h>
  #include <DallasTemperature.h>
#else
  #error "Define either SENSOR_DHT22 or SENSOR_DS18B20"
#endif

/**************************************************************
 *                    CONFIG / PIN MAP
 **************************************************************/
static const char* FW_VERSION  = "1.0.0";
static const uint8_t API_VERSION = 1;

static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "tempnode";

// ===== ESP32 DevKit pin map =====
static const int PIN_I2C_SDA   = 21;
static const int PIN_I2C_SCL   = 22;

static const int PIN_SENSOR    = 5;  // DHT22 data pin  OR  DS18B20 data pin

static const int PIN_BTN_LIGHT = 13; // to GND, INPUT_PULLUP (avoid GPIO2 strapping pin)
static const int PIN_BTN_UP    = 14; // to GND, INPUT_PULLUP (avoid GPIO3/RX)
static const int PIN_BTN_DN    = 4;  // to GND, INPUT_PULLUP

// LCD
static const uint8_t LCD_ADDR  = 0x27;
static const uint8_t LCD_COLS  = 20;
static const uint8_t LCD_ROWS  = 4;

// Filesystem
static const char* FS_ROOT     = "/www";
static const char* INDEX_PATH  = "/www/index.html";
static const char* AP_INDEX_PATH = "/www/ap.html";

// AP captive portal
static const char* AP_PREFIX   = "Temp-Node-";
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_GW(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);
static const uint16_t DNS_PORT = 53;

// Timing
static const uint32_t TICK_BTN_MS    = 10;
static const uint32_t TICK_SENSOR_MS = 2000;  // DHT22 max rate = 0.5 Hz
static const uint32_t TICK_LCD_MS    = 500;
static const uint32_t TICK_MQTT_MS   = 1000;

// Button thresholds
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t LONG_MS     = 2000;
static const uint32_t VLONG_MS    = 9000;

// Optional: set 0 to compile without MQTT
#define ENABLE_MQTT 1

/**************************************************************
 *                      FACTORY PAGES
 **************************************************************/
static const char FACTORY_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temp Node - Factory</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px;line-height:1.4}
.card{max-width:720px;padding:16px;border:1px solid #ddd;border-radius:12px}
code{background:#f3f3f3;padding:2px 6px;border-radius:6px}
a{display:inline-block;margin:6px 0}
</style></head><body>
<div class="card">
<h2>Temp Node (Factory Page)</h2>
<p>UI files are missing or broken. You can still use APIs and the file manager.</p>
<ul>
<li><a href="/setup">Wi-Fi Setup</a></li>
<li><a href="/provision">Connect to Platform</a></li>
<li><a href="/api/status">/api/status</a></li>
<li><a href="/api/temp">/api/temp</a></li>
<li><a href="/files">File Manager</a></li>
</ul>
<p>Expected UI entry file: <code>/www/index.html</code></p>
</div>
</body></html>
)HTML";

static const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temp Node - Wi-Fi Setup</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px}
.card{max-width:520px;padding:16px;border:1px solid #ddd;border-radius:12px}
input{width:100%;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:10px}
button{width:100%;padding:12px;border:0;border-radius:10px;background:#2a8cff;color:white;font-weight:700}
small{color:#666}
</style></head><body>
<div class="card">
<h2>Wi-Fi Setup</h2>
<p><small>Enter your Wi-Fi credentials. Device will reboot and try to join.</small></p>
<form method="POST" action="/api/settings/wifi">
<label>SSID</label>
<input name="ssid" placeholder="Your Wi-Fi SSID" required>
<label>Password</label>
<input name="pass" placeholder="Your Wi-Fi password" type="password">
<button type="submit">Save &amp; Reboot</button>
</form>
<p style="margin-top:12px"><a href="/factory">Factory Page</a></p>
</div>
</body></html>
)HTML";

static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temp Node - Provision</title>
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
<p><small>Enter your topic and API key from the iot.unitani.com. MQTT credentials will be configured automatically.</small></p>
<label>Topic</label>
<input id="topic" name="topic" placeholder="e.g. myfarm-temp1" required>
<label>API Key</label>
<input id="apikey" name="api_key" type="password" placeholder="Paste API key from dashboard" required>
<button id="btn" onclick="doProvision()">Connect</button>
<div id="msg"></div>
<p style="margin-top:12px"><a href="/">Home</a></p>
</div>
<script>
async function doProvision() {
  const topic = document.getElementById('topic').value.trim();
  const apikey = document.getElementById('apikey').value.trim();
  const btn = document.getElementById('btn');
  const msg = document.getElementById('msg');
  if (!topic || !apikey) { showMsg('Topic and API key are required.', false); return; }
  btn.disabled = true; btn.textContent = 'Connecting...';
  msg.style.display = 'none';
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

/**************************************************************
 *                      GLOBAL OBJECTS
 **************************************************************/
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;
static char s_apPass[13]; // "tempnode" + 4-char MAC suffix

#if defined(SENSOR_DHT22)
DHT dht(PIN_SENSOR, DHT22);
#elif defined(SENSOR_DS18B20)
OneWire oneWire(PIN_SENSOR);
DallasTemperature dsSensor(&oneWire);
#endif

#if ENABLE_MQTT
#include <HTTPUpdate.h>
static esp_mqtt_client_handle_t mqttHandle = nullptr;
static volatile bool            mqttConnected = false;
static String _otaTopic;

static void performOTA(const String& url) {
  Serial.printf("[OTA] Starting from: %s\n", url.c_str());
  WiFiClient httpClient;
  httpUpdate.rebootOnUpdate(true);
  auto ret = httpUpdate.update(httpClient, url);
  if (ret == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
}
#endif

/**************************************************************
 *                      DATA MODELS
 **************************************************************/
struct TempReading {
  float temperature = NAN;   // degrees Celsius
  float humidity    = NAN;   // % RH (DHT22 only; NAN for DS18B20)
  bool  valid       = false;
  uint32_t ts_ms    = 0;
  uint8_t  err_count = 0;
};

struct WifiStatus {
  enum Mode : uint8_t { WIFI_OFF=0, WIFI_AP=1, WIFI_STA=2 } mode = WIFI_OFF;
  bool connected = false;
  String ssid = "";
  String ip = "";
  int rssi = -127;
};

struct MqttConfig {
  bool enabled = false;
  String uri = "";
  String user = "";
  String pass = "";
  String base_topic = "tempnode";
  bool retain = true;
  uint16_t pub_period_ms = 5000;  // slower default — sensor updates every 2s
};

struct MqttStatus {
  bool connected = false;
  uint32_t last_pub_ms = 0;
  uint8_t fail_count = 0;
  String last_error = "";
};

/**************************************************************
 *                   APP / UI STATE MACHINE
 **************************************************************/
enum SystemState : uint8_t {
  SYS_BOOT=0,
  SYS_AP_SETUP,
  SYS_CONNECTING,
  SYS_RUNNING
};

enum UIState : uint8_t {
  UI_HOME=0,
  UI_MENU,
  UI_SETUP,
  UI_INFO
};

static SystemState sysState = SYS_BOOT;
static UIState uiState = UI_HOME;
static int menuIndex = 0;
static bool lcdBacklight = true;

static TempReading reading;
static WifiStatus wifiSt;
static MqttConfig mqttCfg;
static MqttStatus mqttSt;

/**************************************************************
 *                         BUTTON ENGINE
 **************************************************************/
enum BtnId  : uint8_t { BTN_LIGHT=1, BTN_UP=2, BTN_DN=3 };
enum EvType : uint8_t { EV_SHORT=1, EV_LONG=2, EV_VLONG=3 };

struct BtnEvent { uint8_t btn; uint8_t type; uint32_t ts; };

struct Btn {
  uint8_t id;
  int pin;

  bool rawLast=false;
  bool stable=false;
  uint32_t rawChangeTs=0;
  uint32_t pressTs=0;
  bool longSent=false;
  bool vlongSent=false;

  Btn() = default;
  Btn(uint8_t _id, int _pin) : id(_id), pin(_pin) {}
};

static Btn bLight{BTN_LIGHT, PIN_BTN_LIGHT};
static Btn bUp   {BTN_UP,    PIN_BTN_UP};
static Btn bDn   {BTN_DN,    PIN_BTN_DN};

static const uint8_t EVQ_SIZE = 10;
static BtnEvent evq[EVQ_SIZE];
static uint8_t evqHead=0, evqTail=0;

static void pushEvent(uint8_t btn, uint8_t type, uint32_t ts) {
  uint8_t nxt = (uint8_t)((evqHead + 1) % EVQ_SIZE);
  if (nxt == evqTail) return;
  evq[evqHead] = {btn, type, ts};
  evqHead = nxt;
}

static bool popEvent(BtnEvent &e) {
  if (evqTail == evqHead) return false;
  e = evq[evqTail];
  evqTail = (uint8_t)((evqTail + 1) % EVQ_SIZE);
  return true;
}

static inline bool readPressed(int pin) {
  return digitalRead(pin) == LOW;
}

static void updateButton(Btn &b, uint32_t now) {
  bool raw = readPressed(b.pin);
  if (raw != b.rawLast) { b.rawLast = raw; b.rawChangeTs = now; }
  if ((now - b.rawChangeTs) < DEBOUNCE_MS) return;

  if (raw != b.stable) {
    b.stable = raw;
    if (b.stable) {
      b.pressTs = now;
      b.longSent = false;
      b.vlongSent = false;
    } else {
      uint32_t held = now - b.pressTs;
      if (!b.longSent && held < LONG_MS) pushEvent(b.id, EV_SHORT, now);
    }
  }

  if (b.stable) {
    uint32_t held = now - b.pressTs;
    if (!b.longSent  && held >= LONG_MS)  { b.longSent  = true; pushEvent(b.id, EV_LONG,  now); }
    if (!b.vlongSent && held >= VLONG_MS) { b.vlongSent = true; pushEvent(b.id, EV_VLONG, now); }
  }
}

static void buttonsInit() {
  pinMode(PIN_BTN_LIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP,    INPUT_PULLUP);
  pinMode(PIN_BTN_DN,    INPUT_PULLUP);
}

/**************************************************************
 *                      LITTLEFS HELPERS
 **************************************************************/
static bool fsInit() {
  if (!LittleFS.begin(true)) { Serial.println("LittleFS mount FAIL"); return false; }
  if (!LittleFS.exists("/www")) {
    if (!LittleFS.mkdir("/www")) { Serial.println("mkdir /www FAIL"); return false; }
  }
  Serial.println("LittleFS OK");
  return true;
}

static bool hasCustomIndex() { return LittleFS.exists(INDEX_PATH); }

static String humanSize(size_t bytes) {
  char buf[32];
  if (bytes < 1024)       { snprintf(buf, sizeof(buf), "%u B",   (unsigned)bytes);           return String(buf); }
  if (bytes < 1024*1024)  { snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);            return String(buf); }
  snprintf(buf, sizeof(buf), "%.1f MB", bytes / 1024.0 / 1024.0);
  return String(buf);
}

/**************************************************************
 *                      STORAGE (NVS)
 **************************************************************/
static void loadMqttConfig() {
  prefs.begin("mqtt", true);
  mqttCfg.enabled       = prefs.getBool("en",    false);
  mqttCfg.uri           = prefs.getString("uri",  "");
  mqttCfg.user          = prefs.getString("user", "");
  mqttCfg.pass          = prefs.getString("pass", "");
  mqttCfg.base_topic    = prefs.getString("topic", "tempnode");
  mqttCfg.retain        = prefs.getBool("ret",   true);
  mqttCfg.pub_period_ms = (uint16_t)prefs.getUShort("per", 5000);
  prefs.end();
}

static void saveMqttConfig(const MqttConfig &c) {
  prefs.begin("mqtt", false);
  prefs.putBool("en",    c.enabled);
  prefs.putString("uri", c.uri);
  prefs.putString("user", c.user);
  prefs.putString("pass", c.pass);
  prefs.putString("topic", c.base_topic);
  prefs.putBool("ret",    c.retain);
  prefs.putUShort("per",  c.pub_period_ms);
  prefs.end();
}

static void clearNetworkAndMqtt() {
#if ENABLE_MQTT
  if (mqttHandle) {
    esp_mqtt_client_stop(mqttHandle);
    esp_mqtt_client_destroy(mqttHandle);
    mqttHandle = nullptr;
    mqttConnected = false;
  }
#endif
  WiFi.disconnect(true, true);
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("mqtt", false);
  prefs.clear();
  prefs.end();
  mqttCfg = MqttConfig{};
}

/**************************************************************
 *                     SENSOR DRIVER
 **************************************************************/
static void sensorInit() {
#if defined(SENSOR_DHT22)
  dht.begin();
#elif defined(SENSOR_DS18B20)
  dsSensor.begin();
  dsSensor.setResolution(12);  // 0.0625°C resolution
#endif
}

static void sensorTick(uint32_t now) {
#if defined(SENSOR_DHT22)
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    reading.err_count++;
    Serial.println("[DHT22] Read error");
    return;
  }
  reading.temperature = t;
  reading.humidity    = h;
  reading.valid       = true;
  reading.ts_ms       = now;

#elif defined(SENSOR_DS18B20)
  dsSensor.requestTemperatures();
  float t = dsSensor.getTempCByIndex(0);

  if (t == DEVICE_DISCONNECTED_C) {
    reading.err_count++;
    Serial.println("[DS18B20] Read error");
    return;
  }
  reading.temperature = t;
  reading.humidity    = NAN;
  reading.valid       = true;
  reading.ts_ms       = now;
#endif

  Serial.printf("[SENSOR] T=%.2f°C  H=%.1f%%\n",
    reading.temperature,
    isnan(reading.humidity) ? 0.0f : reading.humidity);
}

/**************************************************************
 *                          WIFI / AP
 **************************************************************/
static String chipSuffix() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[9];
  snprintf(buf, sizeof(buf), "%08X", (uint32_t)(mac & 0xFFFFFFFF));
  return String(buf);
}

static String deviceName() {
  static String cache;
  if (cache.length()) return cache;
  prefs.begin("net", true);
  String n = prefs.getString("name", "");
  prefs.end();
  cache = n.length() ? n : (String("tempnode-") + chipSuffix().substring(4));
  return cache;
}

static void setDeviceNameIfEmpty() {
  prefs.begin("net", false);
  String n = prefs.getString("name", "");
  if (!n.length()) prefs.putString("name", deviceName());
  prefs.end();
}

static void updateWifiStatus() {
  wifiSt.connected = (WiFi.status() == WL_CONNECTED);
  if (wifiSt.connected) {
    wifiSt.mode = WifiStatus::WIFI_STA;
    wifiSt.ssid = WiFi.SSID();
    wifiSt.ip   = WiFi.localIP().toString();
    wifiSt.rssi = WiFi.RSSI();
  } else {
    wifiSt.mode = (sysState == SYS_AP_SETUP) ? WifiStatus::WIFI_AP : WifiStatus::WIFI_OFF;
    wifiSt.ssid = "";
    wifiSt.ip   = (sysState == SYS_AP_SETUP) ? WiFi.softAPIP().toString() : "";
    wifiSt.rssi = -127;
  }
}

static void startApSetup() {
  sysState = SYS_AP_SETUP;
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  String apSsid = String(AP_PREFIX) + chipSuffix().substring(4);
  WiFi.softAP(apSsid.c_str(), s_apPass[0] ? s_apPass : nullptr);
  dnsServer.start(DNS_PORT, "*", AP_IP);
  updateWifiStatus();
  Serial.printf("[AP] SSID: %s  IP: http://%s\n", apSsid.c_str(), AP_IP.toString().c_str());
}

static void stopApSetup() {
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
}

static void startStaConnect() {
  sysState = SYS_CONNECTING;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  prefs.begin("wifi", true);
  String _ssid = prefs.getString("ssid", "");
  String _pass = prefs.getString("pass", "");
  prefs.end();
  if (_ssid.length()) WiFi.begin(_ssid.c_str(), _pass.c_str());
  else WiFi.begin();
}

static void systemTick(uint32_t now) {
  static uint32_t connectStart = 0;
  static uint8_t tries = 0;

  updateWifiStatus();

  if (sysState == SYS_BOOT) {
    tries = 0;
    connectStart = now;
    startStaConnect();
    return;
  }

  if (sysState == SYS_CONNECTING) {
    if (wifiSt.connected) {
      sysState = SYS_RUNNING;
      Serial.printf("[WiFi] Connected to %s  IP: http://%s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      return;
    }
    if (now - connectStart > 15000) {
      tries++;
      if (tries >= 2) { startApSetup(); }
      else { connectStart = now; WiFi.disconnect(true, false); delay(50); startStaConnect(); }
    }
    return;
  }

  if (sysState == SYS_AP_SETUP) {
    dnsServer.processNextRequest();
    return;
  }
}

/**************************************************************
 *                           MQTT
 **************************************************************/
#if ENABLE_MQTT
static String mqttBase() {
  String base = mqttCfg.base_topic;
  if (!base.length()) base = "tempnode";
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static void mqttPublishNow() {
  if (!mqttConnected) return;
  String base = mqttBase();

  StaticJsonDocument<256> doc;
  doc["ts_ms"] = reading.ts_ms;
  doc["valid"] = reading.valid;
  if (!reading.valid) {
    doc["error"] = "sensor_error";
  } else {
    doc["temperature"] = reading.temperature;
#if defined(SENSOR_DHT22)
    doc["humidity"] = reading.humidity;
    doc["sensor"]   = "DHT22";
#elif defined(SENSOR_DS18B20)
    doc["sensor"] = "DS18B20";
#endif
  }
  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (base + "/telemetry").c_str(), buf, 0, 0, mqttCfg.retain ? 1 : 0);

  if (reading.valid) {
    char tbuf[16]; dtostrf(reading.temperature, 0, 2, tbuf);
    esp_mqtt_client_publish(mqttHandle, (base + "/temperature").c_str(), tbuf, 0, 0, mqttCfg.retain ? 1 : 0);
#if defined(SENSOR_DHT22)
    if (!isnan(reading.humidity)) {
      char hbuf[16]; dtostrf(reading.humidity, 0, 1, hbuf);
      esp_mqtt_client_publish(mqttHandle, (base + "/humidity").c_str(), hbuf, 0, 0, mqttCfg.retain ? 1 : 0);
    }
#endif
  }
}

static void mqttEventHandler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      mqttSt.connected = true;
      mqttSt.last_error = "";
      Serial.println("[MQTT] Connected.");
      {
        String b = mqttBase();
        _otaTopic = b + "/ota/command";
        esp_mqtt_client_subscribe(mqttHandle, _otaTopic.c_str(), 1);
        esp_mqtt_client_publish(mqttHandle, (b + "/status").c_str(), "online", 0, 1, 1);
        mqttPublishNow();
      }
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      mqttSt.connected = false;
      mqttSt.last_error = "disconnected";
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
              Serial.printf("[OTA] New fw %s (have %s)\n", ver, FW_VERSION);
              performOTA(url);
            }
          }
        }
      }
      break;
    default:
      break;
  }
}

static void mqttStart() {
  if (!mqttCfg.enabled || !mqttCfg.uri.length()) return;

  if (mqttHandle) {
    esp_mqtt_client_stop(mqttHandle);
    esp_mqtt_client_destroy(mqttHandle);
    mqttHandle = nullptr;
    mqttConnected = false;
  }

  String clientId = deviceName() + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  String willTopic = mqttBase() + "/status";

  esp_mqtt_client_config_t cfg = {};
  cfg.uri                  = mqttCfg.uri.c_str();
  cfg.client_id            = clientId.c_str();
  cfg.username             = mqttCfg.user.c_str();
  cfg.password             = mqttCfg.pass.c_str();
  cfg.lwt_topic            = willTopic.c_str();
  cfg.lwt_msg              = "offline";
  cfg.lwt_msg_len          = 7;
  cfg.lwt_qos              = 1;
  cfg.lwt_retain           = 1;
  cfg.reconnect_timeout_ms = 5000;
  cfg.cert_pem             = ISRG_ROOT_X1;

  Serial.printf("[MQTT] Starting — uri=%s user=%s\n",
    mqttCfg.uri.c_str(), mqttCfg.user.c_str());

  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

static void mqttTick(uint32_t now) {
  if (!mqttCfg.enabled) { mqttSt.connected = false; return; }
  if (now - mqttSt.last_pub_ms < mqttCfg.pub_period_ms) return;
  mqttSt.last_pub_ms = now;
  mqttPublishNow();
}
#endif

/**************************************************************
 *                       LCD RENDER (cached)
 **************************************************************/
static String lcdCache[4];

static String pad20(const String& s) {
  if (s.length() >= LCD_COLS) return s.substring(0, LCD_COLS);
  String out = s;
  while (out.length() < LCD_COLS) out += " ";
  return out;
}

static void lcdSetLine(uint8_t row, const String& text) {
  if (row >= 4) return;
  String t = pad20(text);
  if (lcdCache[row] != t) {
    lcdCache[row] = t;
    lcd.setCursor(0, row);
    lcd.print(t);
  }
}

static String rssiBars(int rssi) {
  if (rssi > -55) return "||||";
  if (rssi > -65) return "||| ";
  if (rssi > -75) return "||  ";
  if (rssi > -85) return "|   ";
  return "    ";
}

/**************************************************************
 *                         UI HELPERS
 **************************************************************/
static void uiSet(UIState s) { uiState = s; }

static void toggleBacklight() {
  lcdBacklight = !lcdBacklight;
  if (lcdBacklight) lcd.backlight();
  else lcd.noBacklight();
}

static void uiRenderHome() {
  char b0[32], b1[32];

  if (reading.valid) {
#if defined(SENSOR_DHT22)
    snprintf(b0, sizeof(b0), "T:%6.2f%cC H:%4.1f%%",
      reading.temperature, (char)0xDF, reading.humidity);
#elif defined(SENSOR_DS18B20)
    snprintf(b0, sizeof(b0), "Temp: %7.4f%cC",
      reading.temperature, (char)0xDF);
#endif
  } else {
    snprintf(b0, sizeof(b0), "Sensor: reading...");
  }

  const char* w = wifiSt.connected ? "OK" : (sysState == SYS_AP_SETUP ? "AP" : "NO");
#if ENABLE_MQTT
  const char* m = (mqttCfg.enabled && mqttSt.connected) ? "ON" : "OFF";
#else
  const char* m = "OFF";
#endif
  snprintf(b1, sizeof(b1), "WiFi:%s MQTT:%s", w, m);

  String ip = wifiSt.connected ? WiFi.localIP().toString()
                               : (sysState == SYS_AP_SETUP ? WiFi.softAPIP().toString()
                                                           : String("0.0.0.0"));
  String bars = wifiSt.connected ? rssiBars(wifiSt.rssi) : "    ";
  String l2 = "IP:" + ip;
  int rem = LCD_COLS - l2.length();
  if (rem > 0) {
    l2 += " ";
    if ((int)bars.length() > rem - 1) bars = bars.substring(0, rem - 1);
    l2 += bars;
  }

  lcdSetLine(0, b0);
  lcdSetLine(1, b1);
  lcdSetLine(2, l2);
  lcdSetLine(3, "Hold Low = Menu");
}

static void uiRenderMenu() {
  const char* items[] = {"Setup / WiFi", "Info", "Exit"};
  const int n = 3;

  lcdSetLine(0, "== MAIN MENU =======");
  for (int row = 0; row < 3; row++) {
    int idx = row;
    String line = String((idx == menuIndex) ? "> " : "  ") + items[idx];
    lcdSetLine(1 + row, line);
  }
}

static void uiRenderSetup() {
  if (sysState == SYS_AP_SETUP) {
    String apSsid = String(AP_PREFIX) + chipSuffix().substring(4);
    lcdSetLine(0, "== SETUP MODE ======");
    lcdSetLine(1, "SSID:" + apSsid);
    lcdSetLine(2, "PASS:" + String(s_apPass));
    lcdSetLine(3, "Pass:");
  } else {
    lcdSetLine(0, "== WIFI STATUS =====");
    lcdSetLine(1, "SSID:" + wifiSt.ssid);
    lcdSetLine(2, "IP:" + wifiSt.ip);
    lcdSetLine(3, "Hold LIGHT = AP Mode");
  }
}

static void uiRenderInfo() {
  lcdSetLine(0, "== INFO ============");
  lcdSetLine(1, "FW:" + String(FW_VERSION));
  lcdSetLine(2, "Name:" + deviceName());
#if defined(SENSOR_DHT22)
  lcdSetLine(3, "Sensor:DHT22");
#elif defined(SENSOR_DS18B20)
  lcdSetLine(3, "Sensor:DS18B20");
#endif
}

/**************************************************************
 *                      UI EVENT HANDLER
 **************************************************************/
static bool handleGlobalShortcuts(const BtnEvent &e) {
  if (e.btn == BTN_LIGHT && e.type == EV_SHORT) { toggleBacklight(); return true; }
  if (e.btn == BTN_LIGHT && e.type == EV_LONG)  { startApSetup(); uiSet(UI_SETUP); return true; }
  if (e.btn == BTN_LIGHT && e.type == EV_VLONG) {
    clearNetworkAndMqtt();
    delay(100);
    startApSetup();
    uiSet(UI_SETUP);
    return true;
  }
  return false;
}

static void uiHandleEvent(const BtnEvent &e) {
  if (handleGlobalShortcuts(e)) return;

  switch (uiState) {
    case UI_HOME:
      if (e.btn == BTN_DN && e.type == EV_LONG) { uiSet(UI_MENU); menuIndex = 0; }
      break;

    case UI_MENU: {
      const int n = 3;
      if (e.btn == BTN_UP && e.type == EV_SHORT) menuIndex = (menuIndex - 1 + n) % n;
      if (e.btn == BTN_DN && e.type == EV_SHORT) menuIndex = (menuIndex + 1) % n;
      if (e.btn == BTN_DN && e.type == EV_LONG) {
        if (menuIndex == 0)      uiSet(UI_SETUP);
        else if (menuIndex == 1) uiSet(UI_INFO);
        else                     uiSet(UI_HOME);
      }
      if (e.btn == BTN_UP && e.type == EV_LONG) uiSet(UI_HOME);
      break;
    }

    case UI_SETUP:
      if (e.btn == BTN_UP && e.type == EV_LONG) uiSet(UI_MENU);
      if (e.btn == BTN_DN && e.type == EV_LONG) uiSet(UI_HOME);
      break;

    case UI_INFO:
      if (e.btn == BTN_UP && e.type == EV_LONG) uiSet(UI_MENU);
      if (e.btn == BTN_DN && e.type == EV_LONG) uiSet(UI_HOME);
      break;
  }
}

/**************************************************************
 *                    PLATFORM PROVISIONING
 **************************************************************/
static void doProvision(const String& topic, const String& apiKey,
                        AsyncWebServerRequest* req) {
  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;

  String url = String(PLATFORM_API_URL) + "/api/provision";
  http.begin(tls, url);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", apiKey);

  StaticJsonDocument<256> body;
  body["base_topic"] = topic;
  body["node_type"]  = NODE_TYPE;
  String bodyStr;
  serializeJson(body, bodyStr);

  int code = http.POST(bodyStr);
  Serial.printf("[PROV] POST %s → HTTP %d\n", url.c_str(), code);

  if (code == 200 || code == 201) {
    String resp = http.getString();
    http.end();
    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp) == DeserializationError::Ok) {
      JsonObject cfg = doc["mqtt_config"];
      if (cfg) {
        MqttConfig c;
        c.enabled       = true;
        c.uri           = cfg["uri"]   | "";
        c.user          = cfg["user"]  | "";
        c.pass          = cfg["pass"]  | "";
        c.base_topic    = cfg["topic"] | topic.c_str();
        c.retain        = true;
        c.pub_period_ms = 5000;

        Serial.printf("[PROV] uri=%s user=%s topic=%s\n",
          c.uri.c_str(), c.user.c_str(), c.base_topic.c_str());

        mqttCfg = c;
        saveMqttConfig(mqttCfg);
        mqttStart();

        prefs.begin("prov", false);
        prefs.putBool("done",    true);
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

  // Extract error message from platform JSON if possible
  DynamicJsonDocument errDoc(512);
  String errMsg = "http_" + String(code);
  if (deserializeJson(errDoc, errBody) == DeserializationError::Ok) {
    if (errDoc["error"].is<const char*>()) errMsg = errDoc["error"].as<String>();
  }

  String out = "{\"ok\":false,\"error\":\"" + errMsg + "\"}";
  req->send(code == 409 ? 409 : 502, "application/json", out);
}

/**************************************************************
 *                      WEB ROUTES / API
 **************************************************************/
static bool isInApMode() { return sysState == SYS_AP_SETUP; }
static bool allowFileOps() { return isInApMode(); }

static bool isXhr(AsyncWebServerRequest *req) {
  AsyncWebHeader *h = req->getHeader("X-Requested-With");
  return h && h->value().equalsIgnoreCase("XMLHttpRequest");
}

static void sendJson(AsyncWebServerRequest *req, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

static void routeFactoryAndIndex() {
  server.on("/factory", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", FACTORY_HTML);
  });

  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (LittleFS.exists(AP_INDEX_PATH)) req->send(LittleFS, AP_INDEX_PATH, "text/html");
    else req->send_P(200, "text/html", SETUP_HTML);
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    if (hasCustomIndex()) req->send(LittleFS, INDEX_PATH, "text/html");
    else req->send_P(200, "text/html", FACTORY_HTML);
  });

  server.serveStatic("/www/", LittleFS, FS_ROOT);
}

static void routeApi() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<512> doc;
    doc["api"] = API_VERSION;

    JsonObject dev = doc.createNestedObject("device");
    dev["name"]     = deviceName();
    dev["fw"]       = FW_VERSION;
    dev["uptime_s"] = (uint32_t)(millis() / 1000);
#if defined(SENSOR_DHT22)
    dev["sensor"]   = "DHT22";
#elif defined(SENSOR_DS18B20)
    dev["sensor"]   = "DS18B20";
#endif

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["mode"]      = (sysState == SYS_AP_SETUP) ? "AP" : (wifiSt.connected ? "STA" : "OFF");
    wifi["connected"] = wifiSt.connected;
    wifi["ssid"]      = wifiSt.ssid;
    wifi["ip"]        = wifiSt.ip;
    wifi["rssi"]      = wifiSt.rssi;

#if ENABLE_MQTT
    JsonObject mj = doc.createNestedObject("mqtt");
    mj["enabled"]    = mqttCfg.enabled;
    mj["connected"]  = mqttSt.connected;
    mj["last_error"] = mqttSt.last_error;
#endif

    JsonObject fs = doc.createNestedObject("fs");
    fs["total"] = (uint32_t)LittleFS.totalBytes();
    fs["used"]  = (uint32_t)LittleFS.usedBytes();

    sendJson(req, doc);
  });

  server.on("/api/temp", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["ts_ms"]       = reading.ts_ms;
    doc["valid"]       = reading.valid;
    doc["temperature"] = reading.valid ? reading.temperature : NAN;
#if defined(SENSOR_DHT22)
    doc["humidity"]    = reading.valid ? reading.humidity : NAN;
    doc["sensor"]      = "DHT22";
#elif defined(SENSOR_DS18B20)
    doc["sensor"]      = "DS18B20";
#endif
    doc["err_count"] = reading.err_count;
    sendJson(req, doc);
  });

  // Platform provisioning page
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", PROVISION_HTML);
  });

  // Platform provisioning action — takes topic + api_key, fetches MQTT creds
  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *req){
#if !ENABLE_MQTT
    req->send(400, "application/json", "{\"ok\":false,\"error\":\"mqtt_disabled\"}");
    return;
#else
    if (!wifiSt.connected) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"not_connected\"}");
      return;
    }
    if (!req->hasParam("topic", true) || !req->hasParam("api_key", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_params\"}");
      return;
    }
    String topic  = req->getParam("topic",   true)->value();
    String apiKey = req->getParam("api_key", true)->value();
    topic.trim(); apiKey.trim();
    if (!topic.length() || !apiKey.length()) {
      req->send(400, "application/json", "{\"ok\":false,\"error\":\"empty_params\"}");
      return;
    }
    doProvision(topic, apiKey, req);
#endif
  });

  // Wi-Fi credentials
  server.on("/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!isInApMode()) { req->send(403, "text/plain", "only allowed in AP mode"); return; }
    if (!req->hasParam("ssid", true)) { req->send(400, "text/plain", "missing ssid"); return; }
    String ssid = req->getParam("ssid", true)->value();
    String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    req->send(200, "text/html",
      "<html><body><h3>Saved. Rebooting...</h3>"
      "<p>After reconnecting, visit the device IP and go to "
      "<a href='/provision'>/provision</a> to connect to the platform.</p>"
      "</body></html>");
    delay(300);
    ESP.restart();
  });

  // MQTT settings
  server.on("/api/settings/mqtt", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<384> doc;
    doc["enabled"]   = mqttCfg.enabled;
    doc["uri"]       = mqttCfg.uri;
    doc["user"]      = mqttCfg.user;
    doc["pass"]      = "";  // never reveal
    doc["topic"]     = mqttCfg.base_topic;
    doc["retain"]    = mqttCfg.retain;
    doc["period_ms"] = mqttCfg.pub_period_ms;
    sendJson(req, doc);
  });

  server.on("/api/settings/mqtt", HTTP_POST, [](AsyncWebServerRequest *req){
#if !ENABLE_MQTT
    req->send(400, "application/json", "{\"ok\":false,\"err\":\"mqtt_disabled\"}");
    return;
#else
    if (!isXhr(req)) { req->send(403, "application/json", "{\"ok\":false,\"err\":\"csrf\"}"); return; }
    MqttConfig c = mqttCfg;
    if (req->hasParam("enabled",   true)) c.enabled       = (req->getParam("enabled",   true)->value() == "1");
    if (req->hasParam("uri",       true)) c.uri           = req->getParam("uri",       true)->value();
    if (req->hasParam("user",      true)) c.user          = req->getParam("user",      true)->value();
    if (req->hasParam("pass",      true)) c.pass          = req->getParam("pass",      true)->value();
    if (req->hasParam("topic",     true)) c.base_topic    = req->getParam("topic",     true)->value();
    if (req->hasParam("retain",    true)) c.retain        = (req->getParam("retain",    true)->value() == "1");
    if (req->hasParam("period_ms", true)) c.pub_period_ms = (uint16_t)req->getParam("period_ms", true)->value().toInt();
    mqttCfg = c;
    saveMqttConfig(mqttCfg);
    mqttStart();
    req->send(200, "application/json", "{\"ok\":true}");
#endif
  });
}

static void routeFiles() {
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "File manager allowed only in AP mode"); return; }
    String page = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Files</title></head><body style='font-family:system-ui;margin:24px'>"
                  "<h3>File Manager</h3>"
                  "<p>Upload UI files to <code>/www/</code> (index.html, app.js, style.css).</p>"
                  "<form method='POST' action='/upload' enctype='multipart/form-data'>"
                  "<input type='file' name='file'>"
                  "<button type='submit'>Upload</button>"
                  "</form>"
                  "<p><a href='/list'>List files (JSON)</a></p>"
                  "<p><a href='/factory'>Factory</a></p>"
                  "</body></html>";
    req->send(200, "text/html", page);
  });

  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "forbidden"); return; }
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("files");
    File wwwDir = LittleFS.open(FS_ROOT);
    if (!wwwDir || !wwwDir.isDirectory()) {
      doc["ok"] = false; doc["err"] = "open_www_fail";
      sendJson(req, doc); return;
    }
    File f = wwwDir.openNextFile();
    while (f) {
      JsonObject o = arr.createNestedObject();
      o["name"] = String(FS_ROOT) + "/" + String(f.name());
      o["size"] = (uint32_t)f.size();
      f.close();
      f = wwwDir.openNextFile();
    }
    wwwDir.close();
    doc["ok"] = true;
    sendJson(req, doc);
  });

  server.on(
    "/upload", HTTP_POST,
    [](AsyncWebServerRequest *req){
      if (!allowFileOps()) { req->send(403, "text/plain", "forbidden"); return; }
      req->send(200, "text/plain", "Upload OK");
    },
    [](AsyncWebServerRequest *req, String filename, size_t index, uint8_t *data, size_t len, bool final){
      if (!allowFileOps()) return;
      filename.replace("\\", "/");
      int slash = filename.lastIndexOf('/');
      if (slash >= 0) filename = filename.substring(slash + 1);
      if (filename.endsWith(".bin")) return;
      String path = String(FS_ROOT) + "/" + filename;
      if (index == 0) {
        if (req->_tempFile) req->_tempFile.close();
        size_t cl = req->contentLength();
        size_t freeBytes = LittleFS.totalBytes() - LittleFS.usedBytes();
        if (cl > 0 && cl > freeBytes) return;
        req->_tempFile = LittleFS.open(path, "w");
        if (!req->_tempFile) return;
      }
      if (req->_tempFile) {
        req->_tempFile.write(data, len);
        if (final) req->_tempFile.close();
      }
    }
  );

  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "forbidden"); return; }
    if (!req->hasParam("path", true)) { req->send(400, "text/plain", "missing path"); return; }
    String path = req->getParam("path", true)->value();
    if (path.indexOf("..") >= 0 || !path.startsWith(String(FS_ROOT) + "/")) {
      req->send(400, "text/plain", "bad path"); return;
    }
    bool ok = LittleFS.remove(path);
    req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });
}

static void setupWeb() {
  routeFactoryAndIndex();
  routeApi();
  routeFiles();

  server.onNotFound([](AsyncWebServerRequest *req){
    if (isInApMode()) { req->redirect("/setup"); return; }
    if (hasCustomIndex()) {
      String p = req->url();
      if (p == "/") { req->send(LittleFS, INDEX_PATH, "text/html"); return; }
      if (LittleFS.exists(p)) { req->send(LittleFS, p); return; }
      String mapped = String(FS_ROOT) + p;
      if (LittleFS.exists(mapped)) { req->send(LittleFS, mapped); return; }
    }
    req->send_P(404, "text/html", FACTORY_HTML);
  });

  server.begin();
}

/**************************************************************
 *                         LCD INIT
 **************************************************************/
static void lcdInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.clear();
  lcdBacklight = true;
  lcd.backlight();
  for (int i = 0; i < 4; i++) lcdCache[i] = "";
}

static void lcdTick(uint32_t now) {
  (void)now;
  switch (uiState) {
    case UI_HOME:  uiRenderHome();  break;
    case UI_MENU:  uiRenderMenu();  break;
    case UI_SETUP: uiRenderSetup(); break;
    case UI_INFO:  uiRenderInfo();  break;
    default:       uiRenderHome();  break;
  }
}

/**************************************************************
 *                     MQTT INIT (optional)
 **************************************************************/
#if ENABLE_MQTT
static void mqttInit() {
  loadMqttConfig();
  mqttStart();
}
#endif

/**************************************************************
 *                   BOOT / SPLASH HELPERS
 **************************************************************/
static void lcdSplash() {
  lcd.clear();
  lcdSetLine(0, "Temp Node (ESP32)");
  lcdSetLine(1, "FW:" + String(FW_VERSION));
  lcdSetLine(2, "Booting...");
  lcdSetLine(3, "Hold LIGHT = AP");
}

/**************************************************************
 *                           SETUP
 **************************************************************/
void setup() {
  Serial.begin(115200);
  delay(100);

  String _suffix = chipSuffix().substring(4);
  snprintf(s_apPass, sizeof(s_apPass), "");
  Serial.printf("[AP] SSID: Temp-Node-%s\n", _suffix.c_str());

  setDeviceNameIfEmpty();

  bool fsOk = fsInit();

  lcdInit();
  buttonsInit();
  lcdSplash();

  sensorInit();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

#if ENABLE_MQTT
  mqttInit();
#endif

  setupWeb();

  sysState = SYS_BOOT;
  uiState  = UI_HOME;

  if (!fsOk) {
    lcdSetLine(2, "LittleFS FAIL");
    lcdSetLine(3, "Factory only");
    delay(800);
  } else {
    lcdSetLine(2, "LittleFS OK");
    lcdSetLine(3, "Web ready");
    delay(1000);
  }
}

/**************************************************************
 *                            LOOP
 **************************************************************/
void loop() {
  const uint32_t now = millis();

  static uint32_t tBtn = 0, tSys = 0, tSens = 0, tLcd = 0, tMqtt = 0;

  if (now - tBtn >= TICK_BTN_MS) {
    tBtn = now;
    updateButton(bLight, now);
    updateButton(bUp,    now);
    updateButton(bDn,    now);
    BtnEvent e;
    while (popEvent(e)) uiHandleEvent(e);
  }

  if (now - tSys >= 50) {
    tSys = now;
    systemTick(now);
  }

  if (now - tSens >= TICK_SENSOR_MS) {
    tSens = now;
    sensorTick(now);
  }

#if ENABLE_MQTT
  if (now - tMqtt >= TICK_MQTT_MS) {
    tMqtt = now;
    mqttTick(now);
  }
#endif

  if (now - tLcd >= TICK_LCD_MS) {
    tLcd = now;
    lcdTick(now);
  }
}
