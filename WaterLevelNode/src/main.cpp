/**************************************************************
 * WaterLevel-Node for ESP32-C3 SuperMini
 * - 3 buttons: LIGHT/MODE, CAL/UP, ENTER/DN
 * - LCD 20x4 I2C (0x27)
 * - Captive-portal style AP fallback for Wi-Fi provisioning
 * - LittleFS serves /www UI files + file manager endpoints
 * - Always-available JSON APIs: /api/status, /api/level, /api/cal
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

// -------------------- MQTT (ESP-IDF native, WSS+TLS) --------------------
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
 *                    CONFIG / PIN MAP
 **************************************************************/
static const char* FW_VERSION  = "1.0.0";
static const uint8_t API_VERSION = 1;

static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "waterlevelnode";

// ===== ESP32-C3 SuperMini default pin map =====
static const int PIN_I2C_SDA   = 8;
static const int PIN_I2C_SCL   = 9;

static const int PIN_LEVEL_ADC = 0;  // ADC1 recommended on C3 (0-3.3V input)
static const int PIN_BTN_LIGHT = 2;  // to GND, INPUT_PULLUP
static const int PIN_BTN_UP    = 3;  // to GND, INPUT_PULLUP
static const int PIN_BTN_DN    = 4;  // to GND, INPUT_PULLUP

// Optional: For 5V sensor with voltage divider (47k/47k)
static const float VOLTAGE_DIVIDER_RATIO = 2.0f; // 47k/47k = 2:1 divider

// LCD
static const uint8_t LCD_ADDR  = 0x27;
static const uint8_t LCD_COLS  = 20;
static const uint8_t LCD_ROWS  = 4;

// Filesystem
static const char* FS_ROOT     = "/www";
static const char* INDEX_PATH  = "/www/index.html";

// AP captive portal
static const char* AP_PREFIX   = "WaterLevel-";
static const char* AP_PASS     = "12345678"; // you can derive from chip id if you want
static const IPAddress AP_IP(192,168,4,1);
static const IPAddress AP_GW(192,168,4,1);
static const IPAddress AP_MASK(255,255,255,0);
static const uint16_t DNS_PORT = 53;
static const char* AP_INDEX_PATH = "/www/ap.html";

// Timing
static const uint32_t TICK_BTN_MS    = 10;
static const uint32_t TICK_SENSOR_MS = 200;
static const uint32_t TICK_LCD_MS    = 250;
static const uint32_t TICK_MQTT_MS   = 1000;

// Button thresholds
static const uint32_t DEBOUNCE_MS = 30;
static const uint32_t LONG_MS     = 2000;
static const uint32_t VLONG_MS    = 9000;

// ADC sampling
static const uint8_t  ADC_SAMPLES_PER_TICK = 20;
static const float    ADC_REF_VOLTAGE = 3.3f; // ESP32-C3 ADC reference voltage
static const float    MIN_VOLTAGE_DELTA = 0.05f; // calibration voltage delta min

// Measurement units
enum LevelUnit : uint8_t { UNIT_PERCENT=0, UNIT_VOLTAGE=1, UNIT_RAW=2, UNIT_CUSTOM=3 };

// Optional: set 0 to compile without MQTT
#define ENABLE_MQTT 1

/**************************************************************
 *                      FACTORY PAGES
 **************************************************************/
// Minimal factory recovery page (never depends on LittleFS)
static const char FACTORY_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Water Level Node - Factory</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px;line-height:1.4}
.card{max-width:720px;padding:16px;border:1px solid #ddd;border-radius:12px}
code{background:#f3f3f3;padding:2px 6px;border-radius:6px}
a{display:inline-block;margin:6px 0}
</style></head><body>
<div class="card">
<h2>Water Level Node (Factory Page)</h2>
<p>UI files are missing or broken. You can still use APIs and the file manager.</p>
<ul>
<li><a href="/setup">Wi-Fi Setup</a></li>
<li><a href="/provision">Connect to Platform</a></li>
<li><a href="/api/status">/api/status</a></li>
<li><a href="/api/level">/api/level</a></li>
<li><a href="/api/cal">/api/cal</a></li>
<li><a href="/files">File Manager</a></li>
</ul>
<p>Expected UI entry file: <code>/www/index.html</code></p>
</div>
</body></html>
)HTML";

// Simple built-in Wi-Fi setup page (captive-portal friendly)
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Water Level Node - Wi-Fi Setup</title>
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
<title>Water Level Node - Provision</title>
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
<input id="topic" name="topic" placeholder="e.g. myfarm-waterlevel1" required>
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

/**************************************************************
 *                      GLOBAL OBJECTS
 **************************************************************/
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

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
struct LevelReading {
  uint16_t raw_adc = 0;
  float voltage = 0.0f;      // filtered voltage (after divider if applicable)
  float voltage_sensor = 0.0f; // original sensor voltage (0-5V)
  float level_percent = NAN; // 0-100% calculated
  float level_custom = NAN;  // custom unit (cm, inches, etc.)
  float v_avg_10s = 0.0f;    // ring mean
  float v_std = 0.0f;        // ring std
  uint32_t ts_ms = 0;
};

struct CalPoint { 
  float level = 0.0f;        // level value (percent or custom unit)
  float v = 0.0f;           // voltage at that level
  bool set = false; 
};

enum CalQuality : uint8_t { CAL_NONE=0, CAL_WEAK=1, CAL_OK=2 };

struct Calibration {
  CalPoint empty;            // Point A: Empty/0% level
  CalPoint full;             // Point B: Full/100% level
  float slope = 0.0f;        // level per volt
  float offset = 0.0f;       // level offset
  bool valid = false;
  CalQuality quality = CAL_NONE;
  LevelUnit unit = UNIT_PERCENT;
  float custom_max = 100.0f; // max value for custom units (e.g., 50cm)
};

struct WifiStatus {
  enum Mode : uint8_t { WIFI_OFF=0, WIFI_AP=1, WIFI_STA=2 } mode = WIFI_OFF;
  bool connected = false;
  String ssid = "";
  String ip = "";
  int rssi = -127;
  String mdns = "";
};

struct MqttConfig {
  bool enabled = false;
  String uri = "";
  String user = "";
  String pass = "";
  String base_topic = "waterlevel";
  bool retain = true;
  uint16_t pub_period_ms = 1000;
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
  UI_CAL_MENU,
  UI_CAL_WIZARD,
  UI_INFO,
  UI_UNIT_SETUP
};

enum CalStep : uint8_t {
  CAL_UNIT_SELECT=0,
  CAL_EMPTY_SET,
  CAL_EMPTY_CAPTURE,
  CAL_FULL_SET,
  CAL_FULL_CAPTURE,
  CAL_COMPUTE,
  CAL_DONE,
  CAL_ERROR
};

static SystemState sysState = SYS_BOOT;
static UIState uiState = UI_HOME;

// Menu indices
static int menuIndex = 0;     // Setup, Calibration, Unit, Info, Exit
static int calMenuIndex = 0;  // Wizard, View, Clear, Set Unit, Back

// Calibration wizard working vars
static CalStep calStep = CAL_UNIT_SELECT;
static LevelUnit wizard_unit = UNIT_PERCENT;
static float wizard_custom_max = 100.0f;
static float wizard_empty_level = 0.0f;
static float wizard_full_level = 100.0f;
static bool wizard_capturedEmpty = false;
static bool wizard_capturedFull = false;
static float wizard_vEmpty = 0.0f;
static float wizard_vFull = 0.0f;
static String wizard_err = "";

// Current settings
static LevelUnit current_unit = UNIT_PERCENT;
static float custom_max_value = 100.0f;

// Backlight
static bool lcdBacklight = true;

// Device runtime
static LevelReading reading;
static Calibration cal;
static WifiStatus wifiSt;

static MqttConfig mqttCfg;
static MqttStatus mqttSt;

/**************************************************************
 *                         BUTTON ENGINE
 **************************************************************/
enum BtnId : uint8_t { BTN_LIGHT=1, BTN_UPCAL=2, BTN_DOWNENT=3 };
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
static Btn bUp   {BTN_UPCAL, PIN_BTN_UP};
static Btn bDn   {BTN_DOWNENT, PIN_BTN_DN};

static const uint8_t EVQ_SIZE = 10;
static BtnEvent evq[EVQ_SIZE];
static uint8_t evqHead=0, evqTail=0;

static void pushEvent(uint8_t btn, uint8_t type, uint32_t ts) {
  uint8_t nxt = (uint8_t)((evqHead + 1) % EVQ_SIZE);
  if (nxt == evqTail) return; // drop if full
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
  return digitalRead(pin) == LOW; // buttons to GND with pullup
}

static void updateButton(Btn &b, uint32_t now) {
  bool raw = readPressed(b.pin);
  if (raw != b.rawLast) {
    b.rawLast = raw;
    b.rawChangeTs = now;
  }
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
    if (!b.longSent && held >= LONG_MS) { b.longSent = true; pushEvent(b.id, EV_LONG, now); }
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
    Serial.println("Creating /www ...");
    if (!LittleFS.mkdir("/www")) {
      Serial.println("mkdir /www FAIL");
      return false;
    }
  }

  Serial.println("LittleFS OK");
  return true;
}

static bool hasCustomIndex() {
  return LittleFS.exists(INDEX_PATH);
}

static String humanSize(size_t bytes) {
  char buf[32];
  if (bytes < 1024) { snprintf(buf, sizeof(buf), "%u B", (unsigned)bytes); return String(buf); }
  if (bytes < 1024*1024) { snprintf(buf, sizeof(buf), "%.1f KB", bytes/1024.0); return String(buf); }
  snprintf(buf, sizeof(buf), "%.1f MB", bytes/1024.0/1024.0);
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
  mqttCfg.base_topic    = prefs.getString("topic", "waterlevel");
  mqttCfg.retain        = prefs.getBool("ret",   true);
  mqttCfg.pub_period_ms = (uint16_t)prefs.getUShort("per", 1000);
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

static void loadCalibration() {
  prefs.begin("cal", true);
  
  // Calibration points
  cal.empty.level = prefs.getFloat("E_lvl", 0.0f);
  cal.empty.v = prefs.getFloat("E_v", 0.0f);
  cal.empty.set = prefs.getBool("E_set", false);
  
  cal.full.level = prefs.getFloat("F_lvl", 100.0f);
  cal.full.v = prefs.getFloat("F_v", 0.0f);
  cal.full.set = prefs.getBool("F_set", false);
  
  // Calibration parameters
  cal.slope = prefs.getFloat("slope", 0.0f);
  cal.offset = prefs.getFloat("off", 0.0f);
  cal.valid = prefs.getBool("valid", false);
  cal.quality = (CalQuality)prefs.getUChar("q", (uint8_t)CAL_NONE);
  
  // Unit settings
  cal.unit = (LevelUnit)prefs.getUChar("unit", (uint8_t)UNIT_PERCENT);
  cal.custom_max = prefs.getFloat("cmax", 100.0f);
  
  // Current display settings
  current_unit = (LevelUnit)prefs.getUChar("cur_unit", (uint8_t)UNIT_PERCENT);
  custom_max_value = prefs.getFloat("cur_cmax", 100.0f);
  
  prefs.end();
}

static void saveCalibration() {
  prefs.begin("cal", false);
  
  // Calibration points
  prefs.putFloat("E_lvl", cal.empty.level);
  prefs.putFloat("E_v", cal.empty.v);
  prefs.putBool("E_set", cal.empty.set);
  
  prefs.putFloat("F_lvl", cal.full.level);
  prefs.putFloat("F_v", cal.full.v);
  prefs.putBool("F_set", cal.full.set);
  
  // Calibration parameters
  prefs.putFloat("slope", cal.slope);
  prefs.putFloat("off", cal.offset);
  prefs.putBool("valid", cal.valid);
  prefs.putUChar("q", (uint8_t)cal.quality);
  
  // Unit settings
  prefs.putUChar("unit", (uint8_t)cal.unit);
  prefs.putFloat("cmax", cal.custom_max);
  
  // Current display settings
  prefs.putUChar("cur_unit", (uint8_t)current_unit);
  prefs.putFloat("cur_cmax", custom_max_value);
  
  prefs.end();
}

static void clearCalibration() {
  cal = Calibration{};
  cal.unit = UNIT_PERCENT;
  cal.custom_max = 100.0f;
  current_unit = UNIT_PERCENT;
  custom_max_value = 100.0f;
  
  prefs.begin("cal", false);
  prefs.clear();
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
 *                    CALIBRATION MATH
 **************************************************************/
static bool computeCalibration() {
  if (!cal.empty.set || !cal.full.set) return false;
  
  float dv = fabsf(cal.full.v - cal.empty.v);
  float dl = fabsf(cal.full.level - cal.empty.level);
  
  if (dv < MIN_VOLTAGE_DELTA || dl < 1.0f) return false;

  cal.slope = (cal.full.level - cal.empty.level) / (cal.full.v - cal.empty.v);
  cal.offset = cal.empty.level - cal.slope * cal.empty.v;
  cal.valid = true;

  // Quality heuristic
  if (dl >= 50.0f && dv >= 0.50f) cal.quality = CAL_OK;
  else cal.quality = CAL_WEAK;

  return true;
}

static float voltageToLevel(float v) {
  if (!cal.valid) return NAN;
  
  float level = cal.slope * v + cal.offset;
  
  // Clamp to calibration range
  float min_level = min(cal.empty.level, cal.full.level);
  float max_level = max(cal.empty.level, cal.full.level);
  
  if (level < min_level) level = min_level;
  if (level > max_level) level = max_level;
  
  return level;
}

static float adcToVoltage(uint16_t adc) {
  // Convert ADC reading to voltage at ADC pin
  float v_adc = (adc / 4095.0f) * ADC_REF_VOLTAGE;
  
  // If using voltage divider for 5V sensor, calculate original sensor voltage
  float v_sensor = v_adc * VOLTAGE_DIVIDER_RATIO;
  
  return v_sensor;
}

static String unitToString(LevelUnit unit) {
  switch(unit) {
    case UNIT_PERCENT: return "%";
    case UNIT_VOLTAGE: return "V";
    case UNIT_RAW: return "raw";
    case UNIT_CUSTOM: return "unit";
    default: return "?";
  }
}

/**************************************************************
 *                     SENSOR (ADC) + FILTER
 **************************************************************/
// Ring buffer for ~10 seconds at TICK_SENSOR_MS
static const int RING_N = 50;
static float vRing[RING_N];
static int vRingIdx = 0;
static int vRingCount = 0;

// Simple EMA for display voltage
static float vEma = 0.0f;
static const float EMA_A = 0.2f;

static void sensorInit() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_LEVEL_ADC, ADC_11db); // allows up to ~3.3V range
  
  // Prime ring buffer
  for (int i=0;i<RING_N;i++) vRing[i]=0.0f;
}

static void sensorTick(uint32_t now) {
  uint32_t sum = 0;
  for (int i=0;i<ADC_SAMPLES_PER_TICK;i++) {
    sum += (uint16_t)analogRead(PIN_LEVEL_ADC);
    delayMicroseconds(300);
  }
  uint16_t adc = (uint16_t)(sum / ADC_SAMPLES_PER_TICK);
  
  // Convert to voltage
  float v_sensor = adcToVoltage(adc);
  float v_adc = v_sensor / VOLTAGE_DIVIDER_RATIO; // Voltage at ADC pin
  
  // EMA
  if (vRingCount == 0) vEma = v_sensor;
  vEma = (EMA_A * v_sensor) + ((1.0f - EMA_A) * vEma);
  
  // ring buffer for sensor voltage
  vRing[vRingIdx] = v_sensor;
  vRingIdx = (vRingIdx + 1) % RING_N;
  if (vRingCount < RING_N) vRingCount++;
  
  // Calculate ring mean and std
  float mean = 0.0f;
  for (int i=0;i<vRingCount;i++) mean += vRing[i];
  mean /= max(1, vRingCount);
  
  float var = 0.0f;
  for (int i=0;i<vRingCount;i++) {
    float d = vRing[i] - mean;
    var += d*d;
  }
  var /= max(1, vRingCount);
  float stdev = sqrtf(var);
  
  // Calculate level
  float level_raw = voltageToLevel(vEma);
  
  reading.raw_adc = adc;
  reading.voltage = v_adc;
  reading.voltage_sensor = v_sensor;
  reading.v_avg_10s = mean;
  reading.v_std = stdev;
  reading.ts_ms = now;
  
  if (!isnan(level_raw)) {
    // Convert to selected display unit
    switch(current_unit) {
      case UNIT_PERCENT:
        reading.level_percent = level_raw;
        break;
      case UNIT_VOLTAGE:
        reading.level_custom = v_sensor;
        break;
      case UNIT_RAW:
        reading.level_custom = (float)adc;
        break;
      case UNIT_CUSTOM:
        // Scale from calibration unit to custom display unit
        if (cal.unit == UNIT_PERCENT) {
          // Calibrated in %, convert to custom units
          reading.level_custom = (level_raw / 100.0f) * custom_max_value;
        } else {
          // Calibrated in custom units, keep as is
          reading.level_custom = level_raw;
        }
        break;
    }
  } else {
    reading.level_percent = NAN;
    reading.level_custom = NAN;
  }
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
  prefs.begin("net", true);
  String n = prefs.getString("name", "");
  prefs.end();
  if (n.length()) return n;
  return String("waterlevel-") + chipSuffix().substring(4);
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
    wifiSt.ip = WiFi.localIP().toString();
    wifiSt.rssi = WiFi.RSSI();
  } else {
    if (sysState == SYS_AP_SETUP) wifiSt.mode = WifiStatus::WIFI_AP;
    else wifiSt.mode = WifiStatus::WIFI_OFF;
    wifiSt.ssid = "";
    wifiSt.ip = (sysState == SYS_AP_SETUP) ? WiFi.softAPIP().toString() : "";
    wifiSt.rssi = -127;
  }
}

static void startApSetup() {
  sysState = SYS_AP_SETUP;

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);

  String apSsid = String(AP_PREFIX) + chipSuffix().substring(4);
  WiFi.softAP(apSsid.c_str(), AP_PASS);

  // Captive DNS redirect
  dnsServer.start(DNS_PORT, "*", AP_IP);

  updateWifiStatus();
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
      return;
    }
    // timeout -> AP
    if (now - connectStart > 15000) {
      tries++;
      if (tries >= 2) {
        startApSetup();
      } else {
        connectStart = now;
        WiFi.disconnect(true, false);
        delay(50);
        startStaConnect();
      }
    }
    return;
  }

  if (sysState == SYS_AP_SETUP) {
    dnsServer.processNextRequest();
    // If STA creds become available and user saves them, we reboot (handled by POST).
    return;
  }

  // SYS_RUNNING: nothing special here
}

/**************************************************************
 *                           MQTT
 **************************************************************/
#if ENABLE_MQTT
static String mqttBase() {
  String base = mqttCfg.base_topic;
  if (!base.length()) base = "waterlevel";
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static void mqttPublishNow() {
  if (!mqttConnected) return;
  String base = mqttBase();

  StaticJsonDocument<256> doc;
  doc["ts_ms"]          = reading.ts_ms;
  doc["adc"]            = reading.raw_adc;
  doc["voltage_adc"]    = reading.voltage;
  doc["voltage_sensor"] = reading.voltage_sensor;
  if (!isnan(reading.level_percent)) doc["level_percent"] = reading.level_percent;
  if (!isnan(reading.level_custom))  doc["level_custom"]  = reading.level_custom;
  doc["unit"] = (int)current_unit;
  doc["cal"]  = cal.valid ? (cal.quality == CAL_OK ? "OK" : "WEAK") : "NONE";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (base + "/telemetry").c_str(), buf, 0, 0, mqttCfg.retain ? 1 : 0);

  if (!isnan(reading.level_percent)) {
    char levelbuf[16]; dtostrf(reading.level_percent, 0, 1, levelbuf);
    esp_mqtt_client_publish(mqttHandle, (base + "/level").c_str(), levelbuf, 0, 0, mqttCfg.retain ? 1 : 0);
  }
  char vbuf[16]; dtostrf(reading.voltage_sensor, 0, 2, vbuf);
  esp_mqtt_client_publish(mqttHandle, (base + "/voltage").c_str(), vbuf, 0, 0, mqttCfg.retain ? 1 : 0);
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

  String clientId  = deviceName() + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
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
  // 0..4 bars; avoid block chars that some LCDs render weirdly
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
  char b1[32], b2[32], b3[32];
  
  // Line1: Level + Unit
  String unitStr;
  float displayValue;
  
  switch(current_unit) {
    case UNIT_PERCENT:
      unitStr = "%";
      displayValue = reading.level_percent;
      break;
    case UNIT_VOLTAGE:
      unitStr = "V";
      displayValue = reading.voltage_sensor;
      break;
    case UNIT_RAW:
      unitStr = "raw";
      displayValue = (float)reading.raw_adc;
      break;
    case UNIT_CUSTOM:
      unitStr = "unit";
      displayValue = reading.level_custom;
      break;
  }
  
  if (!isnan(displayValue)) {
    snprintf(b1, sizeof(b1), "Level: %5.1f %s", displayValue, unitStr.c_str());
  } else {
    snprintf(b1, sizeof(b1), "Level: --- %s", unitStr.c_str());
  }
  
  // Line2: Voltage + ADC
  snprintf(b2, sizeof(b2), "V:%1.2f ADC:%4d", reading.voltage_sensor, reading.raw_adc);
  
  // Line3: WiFi + MQTT
  const char* w = wifiSt.connected ? "OK" : (sysState == SYS_AP_SETUP ? "AP" : "NO");
#if ENABLE_MQTT
  const char* m = (mqttCfg.enabled && mqttSt.connected) ? "ON" : "OFF";
#else
  const char* m = "OFF";
#endif
  snprintf(b3, sizeof(b3), "WiFi:%s MQTT:%s", w, m);
  
  // Line4: IP + RSSI bars
  String ip = wifiSt.connected ? WiFi.localIP().toString()
                             : (sysState == SYS_AP_SETUP ? WiFi.softAPIP().toString()
                                                         : String("0.0.0.0"));
  String bars = wifiSt.connected ? rssiBars(wifiSt.rssi) : "    ";
  String l4 = "IP:" + ip;
  
  // Add bars only if there is space
  int rem = LCD_COLS - l4.length();
  if (rem > 0) {
    l4 += " ";
    rem--;
    if ((int)bars.length() > rem) bars = bars.substring(0, rem);
    l4 += bars;
  }
  
  lcdSetLine(0, b1);
  lcdSetLine(1, b2);
  lcdSetLine(2, b3);
  lcdSetLine(3, l4);
}

static void uiRenderMenu() {
  const char* items[] = {"Setup / WiFi", "Calibration", "Unit Settings", "Info", "Exit"};
  const int n = 5;
  
  lcdSetLine(0, "== MAIN MENU =======");
  
  // Show 3 items window starting at base
  int base = menuIndex;
  if (base > n - 3) base = n - 3;  // keep within range (shows last page)
  
  for (int row = 0; row < 3; row++) {
    int idx = base + row;
    String line = String((idx == menuIndex) ? "> " : "  ") + items[idx];
    lcdSetLine(1 + row, line);
  }
}

static void uiRenderSetup() {
  if (sysState == SYS_AP_SETUP) {
    String apSsid = String(AP_PREFIX) + chipSuffix().substring(4);
    lcdSetLine(0, "== SETUP MODE ======");
    lcdSetLine(1, "SSID:" + apSsid);
    lcdSetLine(2, "PASS:" + String(AP_PASS));
    lcdSetLine(3, "IP:" + WiFi.softAPIP().toString());
  } else {
    lcdSetLine(0, "== WIFI STATUS =====");
    lcdSetLine(1, "SSID:" + wifiSt.ssid);
    lcdSetLine(2, "IP:" + wifiSt.ip);
    lcdSetLine(3, "Hold LIGHT = AP Mode");
  }
}

static void uiRenderCalMenu() {
  const char* items[] = {"Wizard", "View cal", "Clear cal", "Unit Setup", "Back"};
  const int n = 5;
  
  lcdSetLine(0, "== CALIBRATION =====");
  
  // Same paging logic as uiRenderMenu()
  int base = calMenuIndex;
  if (base > n - 3) base = n - 3;   // keep base within range for 3-line window
  
  for (int row = 0; row < 3; row++) {
    int idx = base + row;
    String line = String((idx == calMenuIndex) ? "> " : "  ") + items[idx];
    lcdSetLine(1 + row, line);
  }
}

static void uiRenderUnitSetup() {
  char line1[32], line2[32], line3[32], line4[32];
  
  snprintf(line1, sizeof(line1), "== UNIT SETUP =====");
  
  switch(current_unit) {
    case UNIT_PERCENT:
      snprintf(line2, sizeof(line2), "Unit: Percent %%");
      snprintf(line3, sizeof(line3), "Range: 0-100%%");
      break;
    case UNIT_VOLTAGE:
      snprintf(line2, sizeof(line2), "Unit: Voltage");
      snprintf(line3, sizeof(line3), "Range: 0-5.0V");
      break;
    case UNIT_RAW:
      snprintf(line2, sizeof(line2), "Unit: ADC Raw");
      snprintf(line3, sizeof(line3), "Range: 0-4095");
      break;
    case UNIT_CUSTOM:
      snprintf(line2, sizeof(line2), "Unit: Custom");
      snprintf(line3, sizeof(line3), "Max: %.1f", custom_max_value);
      break;
  }
  
  snprintf(line4, sizeof(line4), "Hold Lo=Change");
  
  lcdSetLine(0, line1);
  lcdSetLine(1, line2);
  lcdSetLine(2, line3);
  lcdSetLine(3, line4);
}

static void uiRenderInfo() {
  lcdSetLine(0, "== INFO ============");
  lcdSetLine(1, "FW:" + String(FW_VERSION));
  lcdSetLine(2, "Name:" + deviceName());
  lcdSetLine(3, String("Cal:") + (cal.valid ? "OK" : "NONE"));
}

static void uiRenderCalWizard() {
  char line1[32], line2[32], line3[32], line4[32];
  
  float stab = 0.0f;
  if (reading.v_std > 0.0f) {
    stab = 1.0f - min(1.0f, reading.v_std / 0.05f);
  }
  int bars = (int)roundf(stab * 10.0f);
  String bar = "";
  for (int i=0;i<10;i++) bar += (i < bars ? "#" : ".");
  
  switch (calStep) {
    case CAL_UNIT_SELECT:
      snprintf(line1, sizeof(line1), "Cal Wizard: Unit");
      snprintf(line2, sizeof(line2), "Unit: %s", 
               wizard_unit == UNIT_PERCENT ? "Percent" :
               wizard_unit == UNIT_VOLTAGE ? "Voltage" :
               wizard_unit == UNIT_RAW ? "ADC Raw" : "Custom");
      snprintf(line3, sizeof(line3), "Up/Dn: Change");
      snprintf(line4, sizeof(line4), "Hold Lo = Next");
      break;
      
    case CAL_EMPTY_SET:
      snprintf(line1, sizeof(line1), "Set Empty Value");
      snprintf(line2, sizeof(line2), "Empty: %.1f", wizard_empty_level);
      snprintf(line3, sizeof(line3), "Up/Dn: Adjust");
      snprintf(line4, sizeof(line4), "Hold Lo = Next");
      break;
      
    case CAL_EMPTY_CAPTURE:
      snprintf(line1, sizeof(line1), "Capture EMPTY");
      snprintf(line2, sizeof(line2), "V:%1.2f Avg:%1.2f", 
               reading.voltage_sensor, reading.v_avg_10s);
      snprintf(line3, sizeof(line3), "Stable:%s", bar.c_str());
      snprintf(line4, sizeof(line4), "Hold Lo = Capture");
      break;
      
    case CAL_FULL_SET:
      snprintf(line1, sizeof(line1), "Set Full Value");
      if (wizard_unit == UNIT_CUSTOM) {
        snprintf(line2, sizeof(line2), "Full: %.1f (max)", wizard_full_level);
      } else {
        snprintf(line2, sizeof(line2), "Full: %.1f", wizard_full_level);
      }
      snprintf(line3, sizeof(line3), "Up/Dn: Adjust");
      snprintf(line4, sizeof(line4), "Hold Lo = Next");
      break;
      
    case CAL_FULL_CAPTURE:
      snprintf(line1, sizeof(line1), "Capture FULL");
      snprintf(line2, sizeof(line2), "V:%1.2f Avg:%1.2f", 
               reading.voltage_sensor, reading.v_avg_10s);
      snprintf(line3, sizeof(line3), "Stable:%s", bar.c_str());
      snprintf(line4, sizeof(line4), "Hold Lo = Capture");
      break;
      
    case CAL_COMPUTE:
      snprintf(line1, sizeof(line1), "Computing...");
      snprintf(line2, sizeof(line2), "Empty:%1.1f@%1.2fV", 
               wizard_empty_level, wizard_vEmpty);
      snprintf(line3, sizeof(line3), "Full:%1.1f@%1.2fV", 
               wizard_full_level, wizard_vFull);
      snprintf(line4, sizeof(line4), "Please wait");
      break;
      
    case CAL_DONE: {
      const char* q = (cal.quality==CAL_OK) ? "OK" : "WEAK";
      snprintf(line1, sizeof(line1), "CAL SAVED (%s)", q);
      snprintf(line2, sizeof(line2), "Empty:%1.1f", cal.empty.level);
      snprintf(line3, sizeof(line3), "Full:%1.1f", cal.full.level);
      snprintf(line4, sizeof(line4), "Hold Lo = Exit");
      break;
    }
      
    case CAL_ERROR:
      snprintf(line1, sizeof(line1), "CAL ERROR");
      snprintf(line2, sizeof(line2), "%s", wizard_err.c_str());
      snprintf(line3, sizeof(line3), "Try again");
      snprintf(line4, sizeof(line4), "Hold Lo=Back");
      break;
  }
  
  lcdSetLine(0, line1);
  lcdSetLine(1, line2);
  lcdSetLine(2, line3);
  lcdSetLine(3, line4);
}

/**************************************************************
 *                      UI EVENT HANDLER
 **************************************************************/
static float clampLevel(float x, float min_val, float max_val) { 
  return max(min_val, min(max_val, x)); 
}

static void startWizard() {
  uiSet(UI_CAL_WIZARD);
  calStep = CAL_UNIT_SELECT;
  wizard_unit = cal.unit;
  wizard_custom_max = cal.custom_max;
  wizard_empty_level = cal.empty.set ? cal.empty.level : 0.0f;
  wizard_full_level = cal.full.set ? cal.full.level : 100.0f;
  wizard_capturedEmpty = wizard_capturedFull = false;
  wizard_vEmpty = wizard_vFull = 0.0f;
  wizard_err = "";
}

static void handleGlobalShortcuts(const BtnEvent &e) {
  // LIGHT button global actions
  if (e.btn == BTN_LIGHT && e.type == EV_SHORT) {
    toggleBacklight();
    return;
  }
  if (e.btn == BTN_LIGHT && e.type == EV_LONG) {
    // Force AP fallback
    startApSetup();
    uiSet(UI_SETUP);
    return;
  }
  if (e.btn == BTN_LIGHT && e.type == EV_VLONG) {
    // Factory reset network+MQTT
    clearNetworkAndMqtt();
    delay(100);
    startApSetup();
    uiSet(UI_SETUP);
    return;
  }
}

static void uiHandleEvent(const BtnEvent &e) {
  // Global shortcuts first
  handleGlobalShortcuts(e);
  
  switch (uiState) {
    case UI_HOME:
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) { uiSet(UI_CAL_MENU); calMenuIndex = 0; }
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) { uiSet(UI_MENU); menuIndex = 0; }
      break;
      
    case UI_MENU:{
      const int n = 5;
      if (e.btn == BTN_UPCAL && e.type == EV_SHORT) menuIndex = (menuIndex - 1 + n) % n;
      if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) menuIndex = (menuIndex + 1) % n;
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
        if (menuIndex == 0) uiSet(UI_SETUP);
        else if (menuIndex == 1) { uiSet(UI_CAL_MENU); calMenuIndex = 0; }
        else if (menuIndex == 2) uiSet(UI_UNIT_SETUP);
        else if (menuIndex == 3) uiSet(UI_INFO);
        else uiSet(UI_HOME);
      }
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) uiSet(UI_HOME); // back
      break;
    }
    
    case UI_SETUP:
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) uiSet(UI_MENU); // back
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) uiSet(UI_HOME);
      break;
      
    case UI_INFO:
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) uiSet(UI_MENU);
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) uiSet(UI_HOME);
      break;
      
    case UI_UNIT_SETUP:
      if (e.btn == BTN_UPCAL && e.type == EV_SHORT) {
        current_unit = (LevelUnit)((current_unit + 1) % 4);
        if (current_unit == UNIT_CUSTOM && cal.custom_max > 0) {
          custom_max_value = cal.custom_max;
        }
        saveCalibration();
      }
      if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) {
        if (current_unit == UNIT_CUSTOM) {
          custom_max_value -= 10.0f;
          if (custom_max_value < 10.0f) custom_max_value = 10.0f;
          saveCalibration();
        }
      }
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
        uiSet(UI_HOME);
      }
      break;
      
    case UI_CAL_MENU: {
      const int n = 5;
      if (e.btn == BTN_UPCAL && e.type == EV_SHORT) calMenuIndex = (calMenuIndex - 1 + n) % n;
      if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) calMenuIndex = (calMenuIndex + 1) % n;
      
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
        if (calMenuIndex == 0) startWizard();
        else if (calMenuIndex == 1) {
          // View cal
          uiSet(UI_CAL_WIZARD);
          calStep = CAL_DONE;
        } else if (calMenuIndex == 2) {
          clearCalibration();
          uiSet(UI_HOME);
        } else if (calMenuIndex == 3) {
          uiSet(UI_UNIT_SETUP);
        } else {
          uiSet(UI_MENU);
        }
      }
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) uiSet(UI_HOME); // quick exit
      break;
    }
      
    case UI_CAL_WIZARD:
      // CAL long exits wizard anytime
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) { uiSet(UI_HOME); break; }
      
      if (calStep == CAL_UNIT_SELECT) {
        if (e.btn == BTN_UPCAL && e.type == EV_SHORT) {
          wizard_unit = (LevelUnit)((wizard_unit + 1) % 4);
          if (wizard_unit == UNIT_CUSTOM) {
            wizard_full_level = wizard_custom_max;
          }
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) {
          wizard_unit = (LevelUnit)((wizard_unit + 3) % 4); // -1 with wrap
          if (wizard_unit == UNIT_CUSTOM) {
            wizard_full_level = wizard_custom_max;
          }
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          calStep = CAL_EMPTY_SET;
          // Set default values based on unit
          switch(wizard_unit) {
            case UNIT_PERCENT:
              wizard_empty_level = 0.0f;
              wizard_full_level = 100.0f;
              break;
            case UNIT_VOLTAGE:
              wizard_empty_level = 0.0f;
              wizard_full_level = 5.0f;
              break;
            case UNIT_RAW:
              wizard_empty_level = 0.0f;
              wizard_full_level = 4095.0f;
              break;
            case UNIT_CUSTOM:
              wizard_empty_level = 0.0f;
              wizard_full_level = wizard_custom_max;
              break;
          }
        }
      }
      else if (calStep == CAL_EMPTY_SET) {
        float step = (wizard_unit == UNIT_PERCENT) ? 1.0f : 
                    (wizard_unit == UNIT_VOLTAGE) ? 0.1f :
                    (wizard_unit == UNIT_RAW) ? 10.0f : 1.0f;
        
        if (e.btn == BTN_UPCAL && e.type == EV_SHORT) {
          wizard_empty_level += step;
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) {
          wizard_empty_level -= step;
          if (wizard_empty_level < 0) wizard_empty_level = 0;
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          calStep = CAL_EMPTY_CAPTURE;
        }
      }
      else if (calStep == CAL_EMPTY_CAPTURE) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          wizard_vEmpty = reading.v_avg_10s;
          wizard_capturedEmpty = true;
          calStep = CAL_FULL_SET;
        }
      }
      else if (calStep == CAL_FULL_SET) {
        float step = (wizard_unit == UNIT_PERCENT) ? 1.0f : 
                    (wizard_unit == UNIT_VOLTAGE) ? 0.1f :
                    (wizard_unit == UNIT_RAW) ? 10.0f : 1.0f;
        
        if (e.btn == BTN_UPCAL && e.type == EV_SHORT) {
          wizard_full_level += step;
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) {
          wizard_full_level -= step;
          if (wizard_full_level <= wizard_empty_level) wizard_full_level = wizard_empty_level + step;
        }
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          calStep = CAL_FULL_CAPTURE;
        }
      }
      else if (calStep == CAL_FULL_CAPTURE) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          wizard_vFull = reading.v_avg_10s;
          wizard_capturedFull = true;
          calStep = CAL_COMPUTE;
          
          // Compute calibration
          cal.empty.level = wizard_empty_level; 
          cal.empty.v = wizard_vEmpty; 
          cal.empty.set = true;
          cal.full.level = wizard_full_level; 
          cal.full.v = wizard_vFull; 
          cal.full.set = true;
          cal.unit = wizard_unit;
          cal.custom_max = (wizard_unit == UNIT_CUSTOM) ? wizard_full_level : 100.0f;
          
          if (!computeCalibration()) {
            wizard_err = "Points too close";
            cal.valid = false;
            cal.quality = CAL_NONE;
            calStep = CAL_ERROR;
          } else {
            saveCalibration();
            calStep = CAL_DONE;
          }
        }
      }
      else if (calStep == CAL_DONE) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) uiSet(UI_HOME);
      }
      else if (calStep == CAL_ERROR) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) startWizard();
      }
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
        c.pub_period_ms = 1000;

        Serial.printf("[PROV] uri=%s user=%s topic=%s\n",
          c.uri.c_str(), c.user.c_str(), c.base_topic.c_str());

        mqttCfg = c;
        saveMqttConfig(mqttCfg);
        mqttStart();

        prefs.begin("prov", false);
        prefs.putBool("done",     true);
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
  if (deserializeJson(errDoc, errBody) == DeserializationError::Ok) {
    if (errDoc["error"].is<const char*>()) errMsg = errDoc["error"].as<String>();
  }

  String out = "{\"ok\":false,\"error\":\"" + errMsg + "\"}";
  req->send(code == 409 ? 409 : 502, "application/json", out);
}

/**************************************************************
 *                      WEB ROUTES / API
 **************************************************************/
static bool isInApMode() {
  return sysState == SYS_AP_SETUP;
}

// Basic protection for file manager endpoints:
// - allow in AP mode OR require a simple token/pin (future).
static bool allowFileOps() {
  return isInApMode();
}

static void sendJson(AsyncWebServerRequest *req, JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

static void routeFactoryAndIndex() {
  // /factory
  server.on("/factory", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", FACTORY_HTML);
  });
  
  // /setup (always available)
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (LittleFS.exists(AP_INDEX_PATH)) {
      req->send(LittleFS, AP_INDEX_PATH, "text/html");
    } else {
      req->send_P(200, "text/html", SETUP_HTML);
    }
  });
  
  // / (serve custom UI if exists else factory)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    if (hasCustomIndex()) req->send(LittleFS, INDEX_PATH, "text/html");
    else req->send_P(200, "text/html", FACTORY_HTML);
  });
  
  // Serve static under /www (cache optional)
  server.serveStatic("/www/", LittleFS, FS_ROOT);
}

static void routeApi() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<512> doc;
    doc["api"] = API_VERSION;
    
    JsonObject dev = doc.createNestedObject("device");
    dev["name"] = deviceName();
    dev["fw"] = FW_VERSION;
    dev["uptime_s"] = (uint32_t)(millis()/1000);
    
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["mode"] = (sysState==SYS_AP_SETUP) ? "AP" : (wifiSt.connected ? "STA" : "OFF");
    wifi["connected"] = wifiSt.connected;
    wifi["ssid"] = wifiSt.ssid;
    wifi["ip"] = wifiSt.ip;
    wifi["rssi"] = wifiSt.rssi;
    
    JsonObject calj = doc.createNestedObject("cal");
    calj["valid"] = cal.valid;
    calj["quality"] = (cal.quality==CAL_OK) ? "OK" : (cal.quality==CAL_WEAK ? "WEAK" : "NONE");
    calj["unit"] = (int)cal.unit;
    calj["custom_max"] = cal.custom_max;
    calj["current_unit"] = (int)current_unit;
    
#if ENABLE_MQTT
    JsonObject mj = doc.createNestedObject("mqtt");
    mj["enabled"] = mqttCfg.enabled;
    mj["connected"] = mqttSt.connected;
    mj["last_error"] = mqttSt.last_error;
#endif
    
    size_t total = LittleFS.totalBytes();
    size_t used  = LittleFS.usedBytes();
    JsonObject fs = doc.createNestedObject("fs");
    fs["total"] = (uint32_t)total;
    fs["used"]  = (uint32_t)used;
    
    sendJson(req, doc);
  });
  
  server.on("/api/level", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["ts_ms"] = reading.ts_ms;
    doc["adc"] = reading.raw_adc;
    doc["voltage_adc"] = reading.voltage;
    doc["voltage_sensor"] = reading.voltage_sensor;
    if (!isnan(reading.level_percent)) doc["level_percent"] = reading.level_percent;
    if (!isnan(reading.level_custom)) doc["level_custom"] = reading.level_custom;
    doc["v_avg_10s"] = reading.v_avg_10s;
    doc["v_std"] = reading.v_std;
    doc["display_unit"] = (int)current_unit;
    sendJson(req, doc);
  });
  
  server.on("/api/cal", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["valid"] = cal.valid;
    doc["quality"] = (cal.quality==CAL_OK) ? "OK" : (cal.quality==CAL_WEAK ? "WEAK" : "NONE");
    doc["unit"] = (int)cal.unit;
    doc["custom_max"] = cal.custom_max;
    doc["current_unit"] = (int)current_unit;
    
    JsonObject empty = doc.createNestedObject("empty");
    empty["set"] = cal.empty.set; 
    empty["level"] = cal.empty.level; 
    empty["v"] = cal.empty.v;
    
    JsonObject full = doc.createNestedObject("full");
    full["set"] = cal.full.set; 
    full["level"] = cal.full.level; 
    full["v"] = cal.full.v;
    
    sendJson(req, doc);
  });
  
  server.on("/api/cal/clear", HTTP_POST, [](AsyncWebServerRequest *req){
    clearCalibration();
    req->send(200, "application/json", "{\"ok\":true}");
  });
  
  // Unit settings
  server.on("/api/settings/unit", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<128> doc;
    doc["current_unit"] = (int)current_unit;
    doc["custom_max"] = custom_max_value;
    sendJson(req, doc);
  });
  
  server.on("/api/settings/unit", HTTP_POST, [](AsyncWebServerRequest *req){
    if (req->hasParam("current_unit", true)) {
      int unit_val = req->getParam("current_unit", true)->value().toInt();
      if (unit_val >= 0 && unit_val <= 3) {
        current_unit = (LevelUnit)unit_val;
      }
    }
    if (req->hasParam("custom_max", true)) {
      custom_max_value = req->getParam("custom_max", true)->value().toFloat();
      custom_max_value = max(10.0f, custom_max_value);
    }
    saveCalibration();
    req->send(200, "application/json", "{\"ok\":true}");
  });
  
  // Platform provisioning page
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", PROVISION_HTML);
  });

  // Platform provisioning action
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

  // Wi-Fi settings
  server.on("/api/settings/wifi", HTTP_POST, [](AsyncWebServerRequest *req){
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
      "<html><body><h3>Saved. Rebooting...</h3></body></html>");

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
  // /files basic page
  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "File manager allowed only in AP mode"); return; }
    
    String page = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Files</title></head><body style='font-family:system-ui;margin:24px'>"
                  "<h3>File Manager</h3>"
                  "<p>Upload UI files to <code>/www/</code> (index.html, app.js, style.css, images).</p>"
                  "<form method='POST' action='/upload' enctype='multipart/form-data'>"
                  "<input type='file' name='file'>"
                  "<button type='submit'>Upload</button>"
                  "</form>"
                  "<p><a href='/list'>List files (JSON)</a></p>"
                  "<p><a href='/factory'>Factory</a></p>"
                  "</body></html>";
    req->send(200, "text/html", page);
  });
  
  // /list returns JSON listing of /www
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "forbidden"); return; }
    
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.createNestedArray("files");
    
    File wwwDir = LittleFS.open(FS_ROOT);
    if (!wwwDir) { 
      doc["ok"] = false; 
      doc["err"] = "open_www_fail"; 
      sendJson(req, doc); 
      return; 
    }
    
    if (!wwwDir.isDirectory()) {
      doc["ok"] = false;
      doc["err"] = "www_not_dir";
      sendJson(req, doc);
      wwwDir.close();
      return;
    }
    
    File f = wwwDir.openNextFile();
    while (f) {
      JsonObject o = arr.createNestedObject();
      String fullPath = String(FS_ROOT) + "/" + String(f.name());
      o["name"] = fullPath;
      o["size"] = (uint32_t)f.size();
      f.close();
      f = wwwDir.openNextFile();
    }
    wwwDir.close();
    
    doc["ok"] = true;
    sendJson(req, doc);
  });
  
  // Upload handler
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
      
      // Block firmware uploads
      if (filename.endsWith(".bin")) return;
      
      // Force into /www
      String path = String(FS_ROOT) + "/" + filename;
      
      // Start of upload
      if (index == 0) {
        if (req->_tempFile) req->_tempFile.close();
        req->_tempFile = LittleFS.open(path, "w");
        if (!req->_tempFile) return;
      }
      
      // Write chunk
      if (req->_tempFile) {
        req->_tempFile.write(data, len);
        if (final) {
          req->_tempFile.close();
        }
      }
    }
  );
  
  // Delete
  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!allowFileOps()) { req->send(403, "text/plain", "forbidden"); return; }
    if (!req->hasParam("path", true)) { req->send(400, "text/plain", "missing path"); return; }
    String path = req->getParam("path", true)->value();
    if (!path.startsWith(String(FS_ROOT)+"/")) { req->send(400, "text/plain", "bad path"); return; }
    
    bool ok = LittleFS.remove(path);
    req->send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });
}

static void setupWeb() {
  routeFactoryAndIndex();
  routeApi();
  routeFiles();
  
  // Captive portal behavior
  server.onNotFound([](AsyncWebServerRequest *req){
    if (isInApMode()) {
      req->redirect("/setup");
      return;
    }
    
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

/**************************************************************
 *                      UI TICK (render router)
 **************************************************************/
static void lcdTick(uint32_t now) {
  (void)now;
  switch (uiState) {
    case UI_HOME:       uiRenderHome(); break;
    case UI_MENU:       uiRenderMenu(); break;
    case UI_SETUP:      uiRenderSetup(); break;
    case UI_CAL_MENU:   uiRenderCalMenu(); break;
    case UI_CAL_WIZARD: uiRenderCalWizard(); break;
    case UI_INFO:       uiRenderInfo(); break;
    case UI_UNIT_SETUP: uiRenderUnitSetup(); break;
    default:            uiRenderHome(); break;
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
  lcdSetLine(0, "Water Level Node");
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
  
  setDeviceNameIfEmpty();
  
  // FS
  bool fsOk = fsInit();
  
  // LCD + buttons
  lcdInit();
  buttonsInit();
  lcdSplash();
  
  // Load stored configs
  loadCalibration();
  
  // ADC sensor
  sensorInit();
  
  // Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  
#if ENABLE_MQTT
  mqttInit();
#endif
  
  // Web
  setupWeb();
  
  // Start system state machine
  sysState = SYS_BOOT;
  uiState = UI_HOME;
  
  // Show FS status briefly
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
  
  // --- Buttons tick ---
  if (now - tBtn >= TICK_BTN_MS) {
    tBtn = now;
    updateButton(bLight, now);
    updateButton(bUp, now);
    updateButton(bDn, now);
    
    BtnEvent e;
    while (popEvent(e)) {
      uiHandleEvent(e);
    }
  }
  
  // --- System tick ---
  if (now - tSys >= 50) {
    tSys = now;
    systemTick(now);
  }
  
  // --- Sensor tick ---
  if (now - tSens >= TICK_SENSOR_MS) {
    tSens = now;
    sensorTick(now);
  }
  
  // --- MQTT tick ---
#if ENABLE_MQTT
  if (now - tMqtt >= TICK_MQTT_MS) {
    tMqtt = now;
    mqttTick(now);
  }
#endif
  
  // --- LCD tick ---
  if (now - tLcd >= TICK_LCD_MS) {
    tLcd = now;
    lcdTick(now);
  }
}