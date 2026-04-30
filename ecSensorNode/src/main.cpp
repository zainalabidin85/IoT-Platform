/**************************************************************
 * EC-Node (alphaCat-style skeleton) for ESP32-C3 SuperMini
 * - 3 buttons: LIGHT/MODE, CAL/UP, DOWN/ENTER
 * - LCD 20x4 I2C (0x27)
 * - Captive-portal style AP fallback for Wi-Fi provisioning
 * - LittleFS serves /www UI files + file manager endpoints
 * - Always-available JSON APIs: /api/status, /api/ec, /api/cal
 * - MQTT config ONLY via web API (/api/settings/mqtt)
 *
 * Notes:
 * - This is a compilable skeleton intended as a blueprint + base.
 * - UI assets (index.html/app.js/style.css) live in LittleFS under /www/
 * - No OTA firmware update endpoints included (by design).
 **************************************************************/

/***********************
 *  LIBRARIES REQUIRED
 ***********************
 * Arduino core for ESP32 (supports ESP32-C3)
 * LiquidCrystal_I2C
 * ESPAsyncWebServer
 * AsyncTCP
 * DNSServer
 * LittleFS (built-in with ESP32 core)
 * ArduinoJson
 */

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

// -------------------- Platform provisioning + OTA --------------------
#include "mqtt_client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <ESPmDNS.h>

/**************************************************************
 *                    CONFIG / PIN MAP
 **************************************************************/
static const char* FW_VERSION = "1.0.0";
static const uint8_t API_VERSION = 1;
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "ecsensornode";

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

// ===== ESP32-C3 SuperMini default pin map =====
static const int PIN_I2C_SDA   = 8;
static const int PIN_I2C_SCL   = 9;

static const int PIN_EC_ADC    = 0;  // ADC1 recommended on C3
static const int PIN_EC_TEMP   = 1;  // Optional temperature sensor (DS18B20 or thermistor)
static const int PIN_EC_ENABLE = 5;  // Enable pin for EC sensor excitation
static const int PIN_BTN_LIGHT = 2;  // to GND, INPUT_PULLUP
static const int PIN_BTN_UP    = 3;  // to GND, INPUT_PULLUP
static const int PIN_BTN_DN    = 4;  // to GND, INPUT_PULLUP

// LCD
static const uint8_t LCD_ADDR  = 0x27;
static const uint8_t LCD_COLS  = 20;
static const uint8_t LCD_ROWS  = 4;

// Filesystem
static const char* FS_ROOT     = "/www";
static const char* INDEX_PATH  = "/www/index.html";

// AP captive portal
static const char* AP_PREFIX   = "EC-Node-";
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
static const float    CONDUCTANCE_MIN_DELTA = 0.01f; // minimum conductance delta for calibration

// EC sensor parameters
static const float EC_EXCITATION_VOLTAGE = 3.3f;  // Excitation voltage (V)
static const float EC_CELL_CONSTANT = 1.0f;       // Cell constant (cm⁻¹), adjust based on probe
static const float TEMP_COEFFICIENT = 0.019f;     // Temperature coefficient per °C (2% per °C)

// Optional: set 0 to compile without MQTT
#define ENABLE_MQTT 1

/**************************************************************
 *                      FACTORY PAGES
 **************************************************************/
// Minimal factory recovery page (never depends on LittleFS)
static const char FACTORY_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EC Node - Factory</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px;line-height:1.4}
.card{max-width:720px;padding:16px;border:1px solid #ddd;border-radius:12px}
code{background:#f3f3f3;padding:2px 6px;border-radius:6px}
a{display:inline-block;margin:6px 0}
</style></head><body>
<div class="card">
<h2>EC Node (Factory Page)</h2>
<p>UI files are missing or broken. You can still use APIs and the file manager.</p>
<ul>
<li><a href="/setup">Wi-Fi Setup</a></li>
<li><a href="/provision">Platform Provision</a></li>
<li><a href="/api/status">/api/status</a></li>
<li><a href="/api/ec">/api/ec</a></li>
<li><a href="/api/cal">/api/cal</a></li>
<li><a href="/files">File Manager</a></li>
</ul>
<p>Expected UI entry file: <code>/www/index.html</code></p>
</div>
</body></html>
)HTML";

static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EC Node - Platform Provision</title>
<style>
body{font-family:system-ui,Segoe UI,Arial;margin:24px}
.card{max-width:520px;padding:16px;border:1px solid #ddd;border-radius:12px}
input{width:100%;padding:10px;margin:8px 0;box-sizing:border-box;border:1px solid #ccc;border-radius:10px}
button{width:100%;padding:12px;border:0;border-radius:10px;background:#2a8cff;color:white;font-weight:700;cursor:pointer}
button:disabled{background:#aaa}
#msg{margin-top:12px;padding:10px;border-radius:8px;display:none}
.ok{background:#d4edda;color:#155724}.err{background:#f8d7da;color:#721c24}
small{color:#666}
</style></head><body>
<div class="card">
<h2>Connect to Platform</h2>
<p><small>Enter your topic and API key from iot.unitani.com. MQTT credentials will be configured automatically.</small></p>
<label>Topic</label>
<input id="topic" placeholder="e.g. myfarm-ec1" required>
<label>API Key</label>
<input id="apikey" type="password" placeholder="Paste API key from dashboard">
<button id="btn" onclick="doProvision()">Connect</button>
<div id="msg"></div>
<p style="margin-top:12px"><a href="/factory">Factory Page</a></p>
</div>
<script>
async function doProvision(){
  const topic=document.getElementById('topic').value.trim();
  const apikey=document.getElementById('apikey').value.trim();
  const btn=document.getElementById('btn');
  if(!topic||!apikey){showMsg('Topic and API key are required.','err');return;}
  btn.disabled=true;btn.textContent='Connecting...';
  try{
    const fd=new FormData();fd.append('topic',topic);fd.append('api_key',apikey);
    const r=await fetch('/api/provision',{method:'POST',body:fd});
    const j=await r.json();
    if(j.ok){showMsg('Provisioned! MQTT is now configured.','ok');btn.textContent='Done';}
    else{showMsg('Error: '+(j.err||'unknown'),'err');btn.disabled=false;btn.textContent='Connect';}
  }catch(e){showMsg('Request failed: '+e,'err');btn.disabled=false;btn.textContent='Connect';}
}
function showMsg(t,cls){const m=document.getElementById('msg');m.textContent=t;m.className=cls;m.style.display=t?'block':'none';}
</script>
</body></html>
)HTML";

// Simple built-in Wi-Fi setup page (captive-portal friendly)
static const char SETUP_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EC Node - Wi-Fi Setup</title>
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

/**************************************************************
 *                      GLOBAL OBJECTS
 **************************************************************/
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

#if ENABLE_MQTT
static esp_mqtt_client_handle_t mqttHandle = nullptr;
static volatile bool mqttConnected = false;
static String _otaTopic;
#endif

/**************************************************************
 *                      DATA MODELS
 **************************************************************/
struct EcReading {
  uint16_t raw_adc = 0;
  float voltage = 0.0f;           // filtered voltage
  float conductance = 0.0f;       // Siemens (S)
  float ec_raw = 0.0f;            // μS/cm at reference temperature (usually 25°C)
  float ec_temp_comp = 0.0f;      // μS/cm temperature compensated
  float temperature = 25.0f;      // °C
  float g_avg_10s = 0.0f;         // ring mean conductance
  float g_std = 0.0f;             // ring std
  uint32_t ts_ms = 0;
};

struct CalPoint { 
  float ec = 1413.0f;  // μS/cm for 1413 μS/cm calibration solution
  float conductance = 0.0f; 
  bool set = false; 
};

enum CalQuality : uint8_t { CAL_NONE=0, CAL_WEAK=1, CAL_OK=2 };

struct Calibration {
  CalPoint A, B;
  float slope = 0.0f;      // μS/cm per Siemens
  float offset = 0.0f;     // μS/cm offset
  bool valid = false;
  CalQuality quality = CAL_NONE;
  float cell_constant = EC_CELL_CONSTANT;
  float temp_ref = 25.0f;  // Reference temperature for calibration
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
  String base_topic = "ecnode";
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
  UI_TEMP_SETUP
};

enum CalStep : uint8_t {
  CAL_A_SET_EC=0,
  CAL_A_CAPTURE,
  CAL_B_SET_EC,
  CAL_B_CAPTURE,
  CAL_COMPUTE,
  CAL_DONE,
  CAL_ERROR
};

static SystemState sysState = SYS_BOOT;
static UIState uiState = UI_HOME;

// Menu indices
static int menuIndex = 0;     // Setup, Calibration, Temp, Info, Exit
static int calMenuIndex = 0;  // Wizard, View, Clear, Set Cell Const, Back

// Calibration wizard working vars
static CalStep calStep = CAL_A_SET_EC;
static float wizard_ecA = 1413.0f;    // Common calibration point 1413 μS/cm
static float wizard_ecB = 27600.0f;   // Another common point 27600 μS/cm
static float wizard_step = 10.0f;     // Step size for EC adjustment
static bool wizard_capturedA = false;
static bool wizard_capturedB = false;
static float wizard_gA = 0.0f;
static float wizard_gB = 0.0f;
static String wizard_err = "";

// Temperature setup
static float manual_temp = 25.0f;
static bool use_manual_temp = false;

// Backlight
static bool lcdBacklight = true;

// Device runtime
static EcReading reading;
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
  pinMode(PIN_EC_ENABLE, OUTPUT);
  digitalWrite(PIN_EC_ENABLE, HIGH); // Enable EC sensor by default
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
  mqttCfg.enabled = prefs.getBool("en", false);
  mqttCfg.uri = prefs.getString("uri", "");
  mqttCfg.user = prefs.getString("user", "");
  mqttCfg.pass = prefs.getString("pass", "");
  mqttCfg.base_topic = prefs.getString("topic", "ecnode");
  mqttCfg.retain = prefs.getBool("ret", true);
  mqttCfg.pub_period_ms = (uint16_t)prefs.getUShort("per", 1000);
  prefs.end();
}

static void saveMqttConfig(const MqttConfig &c) {
  prefs.begin("mqtt", false);
  prefs.putBool("en", c.enabled);
  prefs.putString("uri", c.uri);
  prefs.putString("user", c.user);
  prefs.putString("pass", c.pass);
  prefs.putString("topic", c.base_topic);
  prefs.putBool("ret", c.retain);
  prefs.putUShort("per", c.pub_period_ms);
  prefs.end();
}

static void loadCalibration() {
  prefs.begin("cal", true);
  cal.A.ec = prefs.getFloat("A_ec", 1413.0f);
  cal.A.conductance = prefs.getFloat("A_g", 0.0f);
  cal.A.set = prefs.getBool("A_set", false);

  cal.B.ec = prefs.getFloat("B_ec", 27600.0f);
  cal.B.conductance = prefs.getFloat("B_g", 0.0f);
  cal.B.set = prefs.getBool("B_set", false);

  cal.slope = prefs.getFloat("slope", 0.0f);
  cal.offset = prefs.getFloat("off", 0.0f);
  cal.valid = prefs.getBool("valid", false);
  cal.quality = (CalQuality)prefs.getUChar("q", (uint8_t)CAL_NONE);
  cal.cell_constant = prefs.getFloat("cell", EC_CELL_CONSTANT);
  cal.temp_ref = prefs.getFloat("tref", 25.0f);
  prefs.end();
}

static void saveCalibration() {
  prefs.begin("cal", false);
  prefs.putFloat("A_ec", cal.A.ec);
  prefs.putFloat("A_g", cal.A.conductance);
  prefs.putBool("A_set", cal.A.set);

  prefs.putFloat("B_ec", cal.B.ec);
  prefs.putFloat("B_g", cal.B.conductance);
  prefs.putBool("B_set", cal.B.set);

  prefs.putFloat("slope", cal.slope);
  prefs.putFloat("off", cal.offset);
  prefs.putBool("valid", cal.valid);
  prefs.putUChar("q", (uint8_t)cal.quality);
  prefs.putFloat("cell", cal.cell_constant);
  prefs.putFloat("tref", cal.temp_ref);
  prefs.end();
}

static void clearCalibration() {
  cal = Calibration{};
  cal.cell_constant = EC_CELL_CONSTANT;
  cal.temp_ref = 25.0f;
  prefs.begin("cal", false);
  prefs.clear();
  prefs.end();
}

static void clearNetworkAndMqtt() {
  WiFi.disconnect(true, true);
#if ENABLE_MQTT
  if (mqttHandle) { esp_mqtt_client_destroy(mqttHandle); mqttHandle = nullptr; mqttConnected = false; }
#endif
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("mqtt", false); prefs.clear(); prefs.end();
  prefs.begin("prov",  false); prefs.clear(); prefs.end();
  mqttCfg = MqttConfig{};
}

static void loadTempConfig() {
  prefs.begin("temp", true);
  use_manual_temp = prefs.getBool("manual", false);
  manual_temp = prefs.getFloat("temp", 25.0f);
  prefs.end();
}

static void saveTempConfig() {
  prefs.begin("temp", false);
  prefs.putBool("manual", use_manual_temp);
  prefs.putFloat("temp", manual_temp);
  prefs.end();
}

/**************************************************************
 *                    CALIBRATION MATH
 **************************************************************/
static bool computeCalibration() {
  if (!cal.A.set || !cal.B.set) return false;
  float dg = fabsf(cal.A.conductance - cal.B.conductance);
  float dec = fabsf(cal.A.ec - cal.B.ec);
  if (dg < CONDUCTANCE_MIN_DELTA || dec < 100.0f) return false;

  cal.slope = (cal.A.ec - cal.B.ec) / (cal.A.conductance - cal.B.conductance);
  cal.offset = cal.A.ec - cal.slope * cal.A.conductance;
  cal.valid = true;

  // Quality heuristic
  if (dec >= 10000.0f && dg >= 0.10f) cal.quality = CAL_OK;
  else cal.quality = CAL_WEAK;

  return true;
}

static float conductanceToEc(float g) {
  if (!cal.valid) return NAN;
  return cal.slope * g + cal.offset;
}

static float temperatureCompensate(float ec, float temp, float ref_temp = 25.0f) {
  // Basic linear temperature compensation
  return ec / (1.0f + TEMP_COEFFICIENT * (temp - ref_temp));
}

/**************************************************************
 *                     SENSOR (ADC) + FILTER
 **************************************************************/
// Ring buffer for ~10 seconds at TICK_SENSOR_MS
static const int RING_N = 50;
static float gRing[RING_N];
static int gRingIdx = 0;
static int gRingCount = 0;

// Simple EMA for display conductance
static float gEma = 0.0f;
static const float EMA_A = 0.2f;

static float readTemperature() {
  // If using DS18B20 or thermistor, implement here
  // For now, return manual temperature or default
  if (use_manual_temp) {
    return manual_temp;
  }
  
  // Optional: Read from temperature sensor on PIN_EC_TEMP
  // For now, return default 25°C
  return 25.0f;
}

static float adcToConductance(uint16_t adc) {
  // Calculate conductance from ADC reading
  // Conductance G = I/V = (V_adc/R_known)/V_excitation
  // With voltage divider: R_unknown = R_known * (V_excitation/V_adc - 1)
  // Conductance G = 1/R_unknown
  
  const float R_KNOWN = 1000.0f;  // Known resistor in series (1kΩ)
  const float ADC_MAX_V = 3.3f;   // ESP32-C3 ADC max voltage
  
  float v_adc = (adc / 4095.0f) * ADC_MAX_V;
  
  // Avoid division by zero
  if (v_adc < 0.001f) v_adc = 0.001f;
  
  // Calculate unknown resistance
  float r_unknown = R_KNOWN * (EC_EXCITATION_VOLTAGE / v_adc - 1.0f);
  
  // Avoid extremely high or low resistances
  if (r_unknown < 1.0f) r_unknown = 1.0f;
  if (r_unknown > 1000000.0f) r_unknown = 1000000.0f;
  
  // Conductance in Siemens (S)
  float conductance = 1.0f / r_unknown;
  
  return conductance;
}

static void sensorInit() {
  analogReadResolution(12);
  analogSetPinAttenuation(PIN_EC_ADC, ADC_11db);
  
  // Initialize temperature reading
  loadTempConfig();
  
  // Prime ring buffer
  for (int i=0;i<RING_N;i++) gRing[i]=0.0f;
}

static void sensorTick(uint32_t now) {
  // Enable EC sensor excitation
  digitalWrite(PIN_EC_ENABLE, HIGH);
  delayMicroseconds(100); // Let sensor stabilize
  
  uint32_t sum = 0;
  for (int i=0;i<ADC_SAMPLES_PER_TICK;i++) {
    sum += (uint16_t)analogRead(PIN_EC_ADC);
    delayMicroseconds(300);
  }
  uint16_t adc = (uint16_t)(sum / ADC_SAMPLES_PER_TICK);
  
  // Disable to reduce electrode polarization
  digitalWrite(PIN_EC_ENABLE, LOW);
  
  float conductance = adcToConductance(adc);
  float voltage = (adc / 4095.0f) * 3.3f;
  
  // EMA
  if (gRingCount == 0) gEma = conductance;
  gEma = (EMA_A * conductance) + ((1.0f - EMA_A) * gEma);
  
  // ring
  gRing[gRingIdx] = conductance;
  gRingIdx = (gRingIdx + 1) % RING_N;
  if (gRingCount < RING_N) gRingCount++;
  
  // ring mean/std
  float mean = 0.0f;
  for (int i=0;i<gRingCount;i++) mean += gRing[i];
  mean /= max(1, gRingCount);
  
  float var = 0.0f;
  for (int i=0;i<gRingCount;i++) {
    float d = gRing[i] - mean;
    var += d*d;
  }
  var /= max(1, gRingCount);
  float stdev = sqrtf(var);
  
  // Read temperature
  float temperature = readTemperature();
  
  // Calculate EC
  float ec_raw = conductanceToEc(gEma);
  float ec_temp_comp = NAN;
  if (!isnan(ec_raw)) {
    ec_temp_comp = temperatureCompensate(ec_raw, temperature, cal.temp_ref);
  }
  
  reading.raw_adc = adc;
  reading.voltage = voltage;
  reading.conductance = gEma;
  reading.ec_raw = ec_raw;
  reading.ec_temp_comp = ec_temp_comp;
  reading.temperature = temperature;
  reading.g_avg_10s = mean;
  reading.g_std = stdev;
  reading.ts_ms = now;
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
  return String("ecnode-") + chipSuffix().substring(4);
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
  WiFi.softAP(apSsid.c_str());

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
      String mdnsName = deviceName();
      if (MDNS.begin(mdnsName.c_str())) {
        MDNS.addService("http", "tcp", 80);
        wifiSt.mdns = mdnsName + ".local";
        Serial.println("[mDNS] http://" + wifiSt.mdns + "/");
      }
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
  if (!base.length()) base = "ecnode";
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static void mqttPublishNow() {
  if (!mqttHandle || !mqttConnected) return;

  String base = mqttBase();
  int qos = 0;
  bool retain = mqttCfg.retain;

  StaticJsonDocument<256> doc;
  doc["ts_ms"] = reading.ts_ms;
  doc["adc"] = reading.raw_adc;
  doc["voltage"] = reading.voltage;
  doc["conductance"] = reading.conductance;
  if (cal.valid) {
    doc["ec_raw"] = reading.ec_raw;
    doc["ec_temp"] = reading.ec_temp_comp;
  }
  doc["temperature"] = reading.temperature;
  doc["cal"] = cal.valid ? (cal.quality == CAL_OK ? "OK" : "WEAK") : "NONE";

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  String telTopic = base + "/telemetry";
  esp_mqtt_client_publish(mqttHandle, telTopic.c_str(), buf, 0, qos, retain);

  if (cal.valid && !isnan(reading.ec_temp_comp)) {
    char ecbuf[24];
    dtostrf(reading.ec_temp_comp, 0, 0, ecbuf);
    String ecTopic = base + "/ec";
    esp_mqtt_client_publish(mqttHandle, ecTopic.c_str(), ecbuf, 0, qos, retain);
  }

  char tempbuf[24]; dtostrf(reading.temperature, 0, 1, tempbuf);
  String tempTopic = base + "/temperature";
  esp_mqtt_client_publish(mqttHandle, tempTopic.c_str(), tempbuf, 0, qos, retain);
}

static void mqttEventHandler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch (event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      mqttSt.fail_count = 0;
      mqttSt.last_error = "";
      esp_mqtt_client_publish(ev->client, (mqttBase() + "/status").c_str(), "online", 0, 1, 1);
      _otaTopic = mqttBase() + "/ota/command";
      esp_mqtt_client_subscribe(ev->client, _otaTopic.c_str(), 1);
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      mqttSt.fail_count++;
      mqttSt.last_error = "disconnected";
      break;
    case MQTT_EVENT_DATA: {
      String topic(ev->topic, ev->topic_len);
      if (topic == _otaTopic) {
        String msg(ev->data, ev->data_len);
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, msg) == DeserializationError::Ok) {
          const char* ver = doc["version"];
          const char* url = doc["url"];
          if (url && ver && String(ver) != FW_VERSION) {
            Serial.printf("[OTA] New fw %s\n", ver);
            WiFiClient httpClient;
            httpUpdate.rebootOnUpdate(true);
            httpUpdate.update(httpClient, url);
          }
        }
      }
      break;
    }
    default: break;
  }
}

static void mqttStart() {
  if (!mqttCfg.enabled || mqttCfg.uri.length() == 0) return;
  if (!wifiSt.connected) return;

  if (mqttHandle) { esp_mqtt_client_destroy(mqttHandle); mqttHandle = nullptr; mqttConnected = false; }

  String base = mqttBase();
  String willTopic = base + "/status";

  esp_mqtt_client_config_t cfg = {};
  cfg.uri = mqttCfg.uri.c_str();
  if (mqttCfg.user.length()) cfg.username = mqttCfg.user.c_str();
  if (mqttCfg.pass.length()) cfg.password = mqttCfg.pass.c_str();
  cfg.client_id = deviceName().c_str();
  cfg.lwt_topic = willTopic.c_str();
  cfg.lwt_msg = "offline";
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;
  cfg.cert_pem = ISRG_ROOT_X1;
  cfg.reconnect_timeout_ms = 5000;

  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

static void mqttTick(uint32_t now) {
  mqttSt.connected = mqttConnected;
  if (!mqttCfg.enabled) { mqttSt.connected = false; return; }
  if (!mqttHandle && wifiSt.connected && mqttCfg.uri.length()) mqttStart();
  if (!mqttConnected) return;
  if (now - mqttSt.last_pub_ms >= mqttCfg.pub_period_ms) {
    mqttSt.last_pub_ms = now;
    mqttPublishNow();
  }
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
  char b1[32], b2[32];
  // Line1: EC + Temperature
  if (cal.valid && !isnan(reading.ec_temp_comp)) {
    snprintf(b1, sizeof(b1), "EC:%5.0f uS/cm", reading.ec_temp_comp);
  } else {
    snprintf(b1, sizeof(b1), "EC:   --- uS/cm");
  }
  snprintf(b2, sizeof(b2), "Temp:%4.1fC G:%1.4fS", reading.temperature, reading.conductance);

  // Line2: WiFi + MQTT
  const char* w = wifiSt.connected ? "OK" : (sysState == SYS_AP_SETUP ? "AP" : "NO");
#if ENABLE_MQTT
  const char* m = (mqttCfg.enabled && mqttSt.connected) ? "ON" : "OFF";
#else
  const char* m = "OFF";
#endif
  char b3[32];
  snprintf(b3, sizeof(b3), "WiFi:%s MQTT:%s", w, m);

  // Line3: IP + RSSI bars
  String ip = wifiSt.connected ? WiFi.localIP().toString()
                             : (sysState == SYS_AP_SETUP ? WiFi.softAPIP().toString()
                                                         : String("0.0.0.0"));

  String bars = wifiSt.connected ? rssiBars(wifiSt.rssi) : "    ";

  String l3 = "IP:" + ip;

  // Add bars only if there is space
  int rem = LCD_COLS - l3.length();
  if (rem > 0) {
    l3 += " ";
    rem--;
    if ((int)bars.length() > rem) bars = bars.substring(0, rem);
    l3 += bars;
  }

  // Line4: hint
  String l4 = "Hold Low = Menu";

  lcdSetLine(0, b1);
  lcdSetLine(1, b2);
  lcdSetLine(2, b3);
  lcdSetLine(3, l4);
}

static void uiRenderMenu() {
  const char* items[] = {"Setup / WiFi", "Calibration", "Temperature", "Info", "Exit"};
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
    lcdSetLine(2, "PASS: none");
    lcdSetLine(3, "IP:" + WiFi.softAPIP().toString());
  } else {
    lcdSetLine(0, "== WIFI STATUS =====");
    lcdSetLine(1, "SSID:" + wifiSt.ssid);
    lcdSetLine(2, "IP:" + wifiSt.ip);
    lcdSetLine(3, "Hold LIGHT = AP Mode");
  }
}

static void uiRenderCalMenu() {
  const char* items[] = {"Wizard", "View cal", "Clear cal", "Cell Constant", "Back"};
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

static void uiRenderInfo() {
  lcdSetLine(0, "== INFO ============");
  lcdSetLine(1, "FW:" + String(FW_VERSION));
  lcdSetLine(2, "Name:" + deviceName());
  lcdSetLine(3, "Cell K:" + String(cal.cell_constant, 2));
}

static void uiRenderTempSetup() {
  char line1[32], line2[32], line3[32], line4[32];
  
  snprintf(line1, sizeof(line1), "== TEMPERATURE ===");
  
  if (use_manual_temp) {
    snprintf(line2, sizeof(line2), "Manual: %4.1fC", manual_temp);
  } else {
    snprintf(line2, sizeof(line2), "Auto: %4.1fC", reading.temperature);
  }
  
  snprintf(line3, sizeof(line3), "Up/Dn: Adjust");
  snprintf(line4, sizeof(line4), "Hold Low: Toggle");
  
  lcdSetLine(0, line1);
  lcdSetLine(1, line2);
  lcdSetLine(2, line3);
  lcdSetLine(3, line4);
}

static void uiRenderCalWizard() {
  char line1[32], line2[32], line3[32], line4[32];

  float stab = 0.0f;
  if (reading.g_std > 0.0f) {
    // crude stability metric: lower std => higher stability
    stab = 1.0f - min(1.0f, reading.g_std / 0.0005f);
  }
  int bars = (int)roundf(stab * 10.0f);
  String bar = "";
  for (int i=0;i<10;i++) bar += (i < bars ? "#" : ".");

  switch (calStep) {
    case CAL_A_SET_EC:
      snprintf(line1, sizeof(line1), "Cal Wizard: Point A");
      snprintf(line2, sizeof(line2), "Set EC A: %.0f uS", wizard_ecA);
      snprintf(line3, sizeof(line3), "Hold Up = Enter");
      snprintf(line4, sizeof(line4), "Hold Low = Exit");
      break;

    case CAL_A_CAPTURE:
      snprintf(line1, sizeof(line1), "Dip sol A %.0fuS", wizard_ecA);
      snprintf(line2, sizeof(line2), "G:%1.4fS Avg:%1.4f", reading.conductance, reading.g_avg_10s);
      snprintf(line3, sizeof(line3), "Stable:%s", bar.c_str());
      snprintf(line4, sizeof(line4), "Hold Low = Enter");
      break;

    case CAL_B_SET_EC:
      snprintf(line1, sizeof(line1), "Cal Wizard: Point B");
      snprintf(line2, sizeof(line2), "Set EC B: %.0f uS", wizard_ecB);
      snprintf(line3, sizeof(line3), "Hold Up = Exit");
      snprintf(line4, sizeof(line4), "Hold Low = Enter");
      break;

    case CAL_B_CAPTURE:
      snprintf(line1, sizeof(line1), "Dip sol B %.0fuS", wizard_ecB);
      snprintf(line2, sizeof(line2), "G:%1.4fS Avg:%1.4f", reading.conductance, reading.g_avg_10s);
      snprintf(line3, sizeof(line3), "Stable:%s", bar.c_str());
      snprintf(line4, sizeof(line4), "Hold Low = Enter");
      break;

    case CAL_COMPUTE:
      snprintf(line1, sizeof(line1), "Computing...");
      snprintf(line2, sizeof(line2), "A:%.0f @ %1.4fS", wizard_ecA, wizard_gA);
      snprintf(line3, sizeof(line3), "B:%.0f @ %1.4fS", wizard_ecB, wizard_gB);
      snprintf(line4, sizeof(line4), "Please wait");
      break;

    case CAL_DONE: {
      const char* q = (cal.quality==CAL_OK) ? "OK" : "WEAK";
      snprintf(line1, sizeof(line1), "CAL SAVED (%s)", q);
      snprintf(line2, sizeof(line2), "A:%.0f @ %1.4fS", cal.A.ec, cal.A.conductance);
      snprintf(line3, sizeof(line3), "B:%.0f @ %1.4fS", cal.B.ec, cal.B.conductance);
      snprintf(line4, sizeof(line4), "Hold Up = Exit");
      break;
    }

    case CAL_ERROR:
      snprintf(line1, sizeof(line1), "CAL ERROR");
      snprintf(line2, sizeof(line2), "%s", wizard_err.c_str());
      snprintf(line3, sizeof(line3), "Try again");
      snprintf(line4, sizeof(line4), "Hold Low = Enter");
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
static float clampEc(float x) { return max(0.0f, min(200000.0f, x)); }
static float clampTemp(float x) { return max(0.0f, min(100.0f, x)); }
static float clampCellConstant(float x) { return max(0.1f, min(10.0f, x)); }

static void startWizard() {
  uiSet(UI_CAL_WIZARD);
  calStep = CAL_A_SET_EC;
  wizard_ecA = cal.A.set ? cal.A.ec : 1413.0f;
  wizard_ecB = cal.B.set ? cal.B.ec : 27600.0f;
  wizard_step = 10.0f;
  wizard_capturedA = wizard_capturedB = false;
  wizard_gA = wizard_gB = 0.0f;
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
        else if (menuIndex == 2) uiSet(UI_TEMP_SETUP);
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

    case UI_TEMP_SETUP:
      if (e.btn == BTN_UPCAL && e.type == EV_SHORT) {
        if (use_manual_temp) {
          manual_temp += 0.5f;
          manual_temp = clampTemp(manual_temp);
          saveTempConfig();
        }
      }
      if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) {
        if (use_manual_temp) {
          manual_temp -= 0.5f;
          manual_temp = clampTemp(manual_temp);
          saveTempConfig();
        }
      }
      if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
        use_manual_temp = !use_manual_temp;
        saveTempConfig();
      }
      if (e.btn == BTN_UPCAL && e.type == EV_LONG) uiSet(UI_MENU);
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
          // Adjust cell constant
          cal.cell_constant += 0.1f;
          if (cal.cell_constant > 10.0f) cal.cell_constant = 0.1f;
          cal.cell_constant = clampCellConstant(cal.cell_constant);
          saveCalibration();
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

      if (calStep == CAL_A_SET_EC) {
        if (e.btn == BTN_UPCAL && e.type == EV_SHORT) wizard_ecA = clampEc(wizard_ecA + wizard_step);
        if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) wizard_ecA = clampEc(wizard_ecA - wizard_step);
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) calStep = CAL_A_CAPTURE;
      }
      else if (calStep == CAL_A_CAPTURE) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          wizard_gA = reading.g_avg_10s;
          wizard_capturedA = true;
          calStep = CAL_B_SET_EC;
        }
      }
      else if (calStep == CAL_B_SET_EC) {
        if (e.btn == BTN_UPCAL && e.type == EV_SHORT) wizard_ecB = clampEc(wizard_ecB + wizard_step);
        if (e.btn == BTN_DOWNENT && e.type == EV_SHORT) wizard_ecB = clampEc(wizard_ecB - wizard_step);
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) calStep = CAL_B_CAPTURE;
      }
      else if (calStep == CAL_B_CAPTURE) {
        if (e.btn == BTN_DOWNENT && e.type == EV_LONG) {
          wizard_gB = reading.g_avg_10s;
          wizard_capturedB = true;
          calStep = CAL_COMPUTE;

          // Compute immediately
          cal.A.ec = wizard_ecA; cal.A.conductance = wizard_gA; cal.A.set = true;
          cal.B.ec = wizard_ecB; cal.B.conductance = wizard_gB; cal.B.set = true;

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

  // /provision
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", PROVISION_HTML);
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

#if ENABLE_MQTT
static void doProvision(const String& base_topic, const String& api_key,
                        AsyncWebServerRequest* req) {
  if (!wifiSt.connected) {
    req->send(400, "application/json", "{\"ok\":false,\"err\":\"not_connected\"}");
    return;
  }

  WiFiClientSecure tls;
  tls.setInsecure();

  HTTPClient http;
  String url = String(PLATFORM_API_URL) + "/api/provision";
  http.begin(tls, url);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", api_key);

  StaticJsonDocument<128> body;
  body["base_topic"] = base_topic;
  body["node_type"] = NODE_TYPE;
  String bodyStr;
  serializeJson(body, bodyStr);

  int code = http.POST(bodyStr);
  Serial.printf("[PROV] POST %s -> HTTP %d\n", url.c_str(), code);

  if (code == 200 || code == 201) {
    String resp = http.getString();
    http.end();

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp) != DeserializationError::Ok) {
      req->send(502, "application/json", "{\"ok\":false,\"err\":\"parse\"}");
      return;
    }

    JsonObject mc = doc["mqtt_config"];
    if (!mc) {
      req->send(502, "application/json", "{\"ok\":false,\"err\":\"no_mqtt_config\"}");
      return;
    }

    MqttConfig c;
    c.enabled       = true;
    c.uri           = mc["uri"]   | "";
    c.user          = mc["user"]  | "";
    c.pass          = mc["pass"]  | "";
    c.base_topic    = mc["topic"] | base_topic.c_str();
    c.retain        = true;
    c.pub_period_ms = 1000;

    Serial.printf("[PROV] uri=%s user=%s topic=%s\n",
      c.uri.c_str(), c.user.c_str(), c.base_topic.c_str());

    mqttCfg = c;
    saveMqttConfig(mqttCfg);
    mqttStart();

    prefs.begin("prov", false);
    prefs.putBool("done", true);
    prefs.putString("topic", mqttCfg.base_topic);
    prefs.end();

    req->send(200, "application/json", "{\"ok\":true}");
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

  String out = "{\"ok\":false,\"err\":\"" + errMsg + "\"}";
  req->send(code == 409 ? 409 : 502, "application/json", out);
}
#endif

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
    calj["cell_constant"] = cal.cell_constant;
    calj["temp_ref"] = cal.temp_ref;

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

  server.on("/api/ec", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["ts_ms"] = reading.ts_ms;
    doc["adc"] = reading.raw_adc;
    doc["voltage"] = reading.voltage;
    doc["conductance"] = reading.conductance;
    if (cal.valid) {
      doc["ec_raw"] = reading.ec_raw;
      doc["ec_temp"] = reading.ec_temp_comp;
    }
    doc["temperature"] = reading.temperature;
    doc["g_avg_10s"] = reading.g_avg_10s;
    doc["g_std"] = reading.g_std;
    sendJson(req, doc);
  });

  server.on("/api/cal", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["valid"] = cal.valid;
    doc["quality"] = (cal.quality==CAL_OK) ? "OK" : (cal.quality==CAL_WEAK ? "WEAK" : "NONE");
    doc["cell_constant"] = cal.cell_constant;
    doc["temp_ref"] = cal.temp_ref;
    JsonObject A = doc.createNestedObject("A");
    A["set"] = cal.A.set; A["ec"] = cal.A.ec; A["conductance"] = cal.A.conductance;
    JsonObject B = doc.createNestedObject("B");
    B["set"] = cal.B.set; B["ec"] = cal.B.ec; B["conductance"] = cal.B.conductance;
    sendJson(req, doc);
  });

  server.on("/api/cal/clear", HTTP_POST, [](AsyncWebServerRequest *req){
    clearCalibration();
    req->send(200, "application/json", "{\"ok\":true}");
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

  // Temperature settings
  server.on("/api/settings/temperature", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<128> doc;
    doc["use_manual"] = use_manual_temp;
    doc["manual_temp"] = manual_temp;
    doc["current_temp"] = reading.temperature;
    sendJson(req, doc);
  });

  server.on("/api/settings/temperature", HTTP_POST, [](AsyncWebServerRequest *req){
    if (req->hasParam("use_manual", true)) {
      use_manual_temp = (req->getParam("use_manual", true)->value() == "1");
    }
    if (req->hasParam("manual_temp", true)) {
      manual_temp = req->getParam("manual_temp", true)->value().toFloat();
      manual_temp = clampTemp(manual_temp);
    }
    saveTempConfig();
    req->send(200, "application/json", "{\"ok\":true}");
  });

#if ENABLE_MQTT
  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!wifiSt.connected) {
      req->send(400, "application/json", "{\"ok\":false,\"err\":\"not_connected\"}");
      return;
    }
    if (!req->hasParam("topic", true) || !req->hasParam("api_key", true)) {
      req->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_params\"}");
      return;
    }
    String topic  = req->getParam("topic",   true)->value();
    String apiKey = req->getParam("api_key", true)->value();
    topic.trim(); apiKey.trim();
    if (!topic.length() || !apiKey.length()) {
      req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty_params\"}");
      return;
    }
    doProvision(topic, apiKey, req);
  });
#endif

  // MQTT settings
  server.on("/api/settings/mqtt", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<384> doc;
    doc["enabled"] = mqttCfg.enabled;
    doc["uri"] = mqttCfg.uri;
    doc["user"] = mqttCfg.user;
    doc["pass"] = "";
    doc["topic"] = mqttCfg.base_topic;
    doc["retain"] = mqttCfg.retain;
    doc["period_ms"] = mqttCfg.pub_period_ms;

    bool provDone = false;
    prefs.begin("prov", true);
    provDone = prefs.getBool("done", false);
    prefs.end();
    doc["provisioned"] = provDone;

    sendJson(req, doc);
  });

  server.on("/api/settings/mqtt", HTTP_POST, [](AsyncWebServerRequest *req){
#if !ENABLE_MQTT
    req->send(400, "application/json", "{\"ok\":false,\"err\":\"mqtt_disabled\"}");
    return;
#else
    MqttConfig c = mqttCfg;

    if (req->hasParam("enabled", true)) c.enabled = (req->getParam("enabled", true)->value() == "1");
    if (req->hasParam("uri", true)) c.uri = req->getParam("uri", true)->value();
    if (req->hasParam("user", true)) c.user = req->getParam("user", true)->value();
    if (req->hasParam("pass", true)) c.pass = req->getParam("pass", true)->value();
    if (req->hasParam("topic", true)) c.base_topic = req->getParam("topic", true)->value();
    if (req->hasParam("retain", true)) c.retain = (req->getParam("retain", true)->value() == "1");
    if (req->hasParam("period_ms", true)) c.pub_period_ms = (uint16_t)req->getParam("period_ms", true)->value().toInt();

    mqttCfg = c;
    saveMqttConfig(mqttCfg);
    if (wifiSt.connected) mqttStart();

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
    case UI_TEMP_SETUP: uiRenderTempSetup(); break;
    default:            uiRenderHome(); break;
  }
}

/**************************************************************
 *                     MQTT INIT (optional)
 **************************************************************/
#if ENABLE_MQTT
static void mqttInit() {
  loadMqttConfig();
}
#endif

/**************************************************************
 *                   BOOT / SPLASH HELPERS
 **************************************************************/
static void lcdSplash() {
  lcd.clear();
  lcdSetLine(0, "EC Node v" + String(FW_VERSION));
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
  loadTempConfig();

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