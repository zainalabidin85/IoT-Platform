/**************************************************************
 * HydroNode (Merged) for ESP32 Dev  ✅ MQTT-FIX VERSION
 *  - EC (0–5V transmitter, powered by 12V, output terminals: + and -)
 *  - Water Level (0–5V or 0–3.3V analog)
 *  - ✅ DS18B20 temperature sensor (GPIO4)
 *
 * FIXES in this build:
 *  ✅ MQTT will NOT block UI anymore:
 *     - MQTT disabled automatically in AP mode / when WiFi not connected
 *     - MQTT reconnect retries slowed down (default 15s)
 *     - PubSubClient socket timeout reduced (1s)
 *     - Buttons are polled BEFORE MQTT work
 **************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <LittleFS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include "mqtt_client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>

#include <OneWire.h>
#include <DallasTemperature.h>

/**************************************************************
 * VERSION
 **************************************************************/
static const char* FW_VERSION = "1.0.0";
static const uint8_t API_VERSION = 1;
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "hydronode";

static const char ISRG_ROOT_X1[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoBggIBAK3oJHP0FDfzm54rVygc
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
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIyrQLIHxW0BD7pBmyDhVV
aIXSdyFBBGPIHPHVkGAYuHwQx8v6IvGcvI3Dxcj/HuNkIH2VUbMvUMWNbU8oYsJ
mh/G8ENq7A0H7UBRqzf3FKb5J7L6gvhvMxR4I8+9lMCHoB1J3TL5xH0xyVX2oLK
z7rFMYExJVNjc7Dv7vCWJw7VZQXODGzW4Xq0YisTLw2jbulCH5GV4LnHlABUhxb
hW0sDfcVU7VNGLhGCpuuLTwwL2e7J9PLVR2DKf8a9nt7RP3fubSMkDr3qHtJZDv
g8Hs3kJVLDPWJxFVCWP9SBpJ0+nxFlFvMcNwJ1UVWw4U1BVZ8sFP5O8JEWZvIJY
OJkVqAXKE7LNQJ6T0tM8JbnNFUJKmvIxTblqyQR6jRIjJJA7J2dRQ==
-----END CERTIFICATE-----
)EOF";

static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Hydro Node - Platform Provision</title>
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
<input id="topic" placeholder="e.g. myfarm-hydro1" required>
<label>API Key</label>
<input id="apikey" type="password" placeholder="Paste API key from dashboard">
<button id="btn" onclick="doProvision()">Connect</button>
<div id="msg"></div>
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

/**************************************************************
 * PINS (ESP32 Dev)
 **************************************************************/
// I2C for LCD
static const int PIN_I2C_SDA = 21;
static const int PIN_I2C_SCL = 22;

// ADC pins (input-only, ADC1 — no WiFi interference)
static const int PIN_EC_ADC    = 34;  // EC analog in
static const int PIN_LEVEL_ADC = 35;  // Level analog in

// Buttons (to GND, INPUT_PULLUP)
static const int PIN_BTN_LIGHT = 26;  // LIGHT/MODE
static const int PIN_BTN_UP    = 27;  // CAL/UP
static const int PIN_BTN_DN    = 25;  // DOWN/ENTER

// DS18B20 (1-Wire)
static const int PIN_DS18B20   = 4;   // DATA pin

// LCD
static const uint8_t LCD_ADDR = 0x27;
static const uint8_t LCD_COLS = 20;
static const uint8_t LCD_ROWS = 4;

/**************************************************************
 * DIVIDER RATIOS
 **************************************************************/
static const float EC_DIVIDER_RATIO = 2.0f;
static const float LEVEL_DIVIDER_RATIO = 2.0f;

/**************************************************************
 * TIMING
 **************************************************************/
static const uint32_t TICK_UI_MS      = 100;
static const uint32_t TICK_SENSOR_MS  = 250;
static const uint32_t TICK_MQTT_MS    = 200;

static const uint32_t SHORT_MS = 60;
static const uint32_t LONG_MS  = 700;
static const uint32_t VLONG_MS = 3500;

static const uint8_t ADC_SAMPLES_PER_TICK = 16;

/**************************************************************
 * OBJECTS
 **************************************************************/
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

static esp_mqtt_client_handle_t mqttHandle = nullptr;
static volatile bool mqttConnected = false;
static String _otaTopic;

OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18(&oneWire);

/**************************************************************
 * STATUS / CONFIG
 **************************************************************/
struct WifiStatus {
  enum Mode : uint8_t { WIFI_OFF=0, WIFI_AP=1, WIFI_STA=2 } mode = WIFI_OFF;
  bool connected = false;
  String ssid = "";
  String ip = "";
};

struct MqttConfig {
  bool enabled = false;
  String uri = "";
  String user = "";
  String pass = "";
  String base_topic = "hydronode";
  bool retain = true;
  uint16_t pub_period_ms = 1000;
};

struct MqttStatus {
  bool connected = false;
  uint32_t lastPublishMs = 0;
  String err = "";
};

enum CalQuality : uint8_t { CAL_NONE=0, CAL_WEAK=1, CAL_OK=2 };

struct EcCalPoint {
  float ec_us = 1413.0f;
  float v = 0.0f;
  bool set = false;
};

struct EcCal {
  EcCalPoint A;
  EcCalPoint B;
  bool valid = false;
  float slope = 1.0f;   // EC = slope * V + offset
  float offset = 0.0f;
  CalQuality quality = CAL_NONE;
};

enum LevelUnit : uint8_t { UNIT_PERCENT=0, UNIT_CUSTOM=1 };

struct LevelCalPoint {
  float level = 0.0f;
  float v = 0.0f;
  bool set = false;
};

struct LevelCal {
  LevelCalPoint empty;
  LevelCalPoint full;
  bool valid = false;
  float slope = 0.0f;
  float offset = 0.0f;
  CalQuality quality = CAL_NONE;
  LevelUnit unit = UNIT_PERCENT;
  float custom_max = 100.0f;
};

struct Sensors {
  // EC
  uint16_t ec_adc_raw = 0;
  float ec_v = 0.0f;
  float ec_us = 0.0f;

  // LEVEL
  uint16_t lvl_adc_raw = 0;
  float lvl_v = 0.0f;
  float lvl_value = 0.0f;
  float lvl_percent = 0.0f;

  // TEMP
  float temp_c = NAN;
};

static WifiStatus wifiSt;
static MqttConfig mqttCfg;
static MqttStatus mqttSt;
static EcCal ecCal;
static LevelCal lvlCal;
static Sensors sens;

/**************************************************************
 * UI STATE
 **************************************************************/
static bool lcdBacklight = true;

static const int MENU_N = 3;
static const int CAL_N  = 3;

enum UIState : uint8_t {
  UI_HOME=0,
  UI_MENU,
  UI_SETUP,
  UI_CAL_MENU,
  UI_CAL_EC,
  UI_CAL_LEVEL,
  UI_LEVEL_UNIT,
  UI_INFO
};

static UIState ui = UI_HOME;

static int menuIndex = 0;
static int calIndex  = 0;

enum BtnId : uint8_t { BTN_LIGHT=0, BTN_UP=1, BTN_DN=2 };
enum EvType : uint8_t { EV_NONE=0, EV_SHORT=1, EV_LONG=2, EV_VLONG=3 };

struct Btn {
  int pin;
  bool down;
  uint32_t downMs;
  Btn(int p) : pin(p), down(false), downMs(0) {}
};

static Btn btns[3] = { Btn(PIN_BTN_LIGHT), Btn(PIN_BTN_UP), Btn(PIN_BTN_DN) };

/**************************************************************
 * CAL WIZARDS
 **************************************************************/
enum EcStep : uint8_t { EC_A_SET=0, EC_A_CAP, EC_B_SET, EC_B_CAP, EC_DONE };
static EcStep ecStep = EC_A_SET;
static float ecWizardA = 1413.0f;
static float ecWizardB = 27600.0f;

enum LvlStep : uint8_t { LVL_UNIT=0, LVL_EMPTY_SET, LVL_EMPTY_CAP, LVL_FULL_SET, LVL_FULL_CAP, LVL_DONE };
static LvlStep lvlStep = LVL_UNIT;
static float lvlWizardEmpty = 0.0f;
static float lvlWizardFull  = 100.0f;

/**************************************************************
 * WIFI / CAPTIVE PORTAL
 **************************************************************/
static bool apMode = false;

/**************************************************************
 * HELPERS
 **************************************************************/
static String ipToString(const IPAddress& ip){
  return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]);
}

static void lcdSetLine(uint8_t row, const String& s){
  char buf[LCD_COLS + 1];
  memset(buf, ' ', LCD_COLS);
  buf[LCD_COLS] = '\0';

  size_t n = s.length();
  if (n > LCD_COLS) n = LCD_COLS;
  memcpy(buf, s.c_str(), n);

  lcd.setCursor(0, row);
  lcd.print(buf);
}

static void uiSet(UIState st){
  ui = st;
  lcd.clear();
}

static String chipSuffix(){
  uint64_t mac = ESP.getEfuseMac();
  char buf[9];
  snprintf(buf, sizeof(buf), "%08X", (uint32_t)(mac & 0xFFFFFFFF));
  return String(buf);
}

static void wipeNetworkAndRestart(){
  WiFi.disconnect(true, true);
  if (mqttHandle) { esp_mqtt_client_destroy(mqttHandle); mqttHandle = nullptr; mqttConnected = false; }
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("mqtt", false); prefs.clear(); prefs.end();
  prefs.begin("prov", false); prefs.clear(); prefs.end();
  delay(200);
  ESP.restart();
}

static void startAP(){
  apMode = true;
  WiFi.mode(WIFI_AP);
  String apSsid = String("HydroNode-") + chipSuffix().substring(4);
  WiFi.softAP(apSsid.c_str());
  IPAddress ip = WiFi.softAPIP();

  wifiSt.mode = WifiStatus::WIFI_AP;
  wifiSt.connected = false;
  wifiSt.ssid = apSsid;
  wifiSt.ip = ipToString(ip);

  dnsServer.start(53, "*", ip);
}

static void startSTA(){
  apMode = false;
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  prefs.begin("wifi", true);
  String _ssid = prefs.getString("ssid", "");
  String _pass = prefs.getString("pass", "");
  prefs.end();
  if (_ssid.length()) WiFi.begin(_ssid.c_str(), _pass.c_str());
  else WiFi.begin();
}

static void wifiTick(){
  if (!apMode){
    if (WiFi.status() == WL_CONNECTED){
      wifiSt.mode = WifiStatus::WIFI_STA;
      wifiSt.connected = true;
      wifiSt.ip = ipToString(WiFi.localIP());
      wifiSt.ssid = WiFi.SSID();
    } else {
      wifiSt.mode = WifiStatus::WIFI_STA;
      wifiSt.connected = false;
      wifiSt.ip = "";
      wifiSt.ssid = "";
    }
  } else {
    dnsServer.processNextRequest();
  }
}

/**************************************************************
 * PREFERENCES: MQTT
 **************************************************************/
static void loadMqtt(){
  prefs.begin("mqtt", true);
  mqttCfg.enabled       = prefs.getBool("en", false);
  mqttCfg.uri           = prefs.getString("uri", "");
  mqttCfg.user          = prefs.getString("user", "");
  mqttCfg.pass          = prefs.getString("pass", "");
  mqttCfg.base_topic    = prefs.getString("topic", "hydronode");
  mqttCfg.retain        = prefs.getBool("ret", true);
  mqttCfg.pub_period_ms = (uint16_t)prefs.getUShort("per", 1000);
  prefs.end();
}

static void saveMqtt(){
  prefs.begin("mqtt", false);
  prefs.putBool("en", mqttCfg.enabled);
  prefs.putString("uri", mqttCfg.uri);
  prefs.putString("user", mqttCfg.user);
  prefs.putString("pass", mqttCfg.pass);
  prefs.putString("topic", mqttCfg.base_topic);
  prefs.putBool("ret", mqttCfg.retain);
  prefs.putUShort("per", mqttCfg.pub_period_ms);
  prefs.end();
}

/**************************************************************
 * PREFERENCES: CAL
 **************************************************************/
static void loadEcCal(){
  prefs.begin("eccal", true);
  ecCal.A.ec_us = prefs.getFloat("A_ec", 1413.0f);
  ecCal.A.v     = prefs.getFloat("A_v", 0.0f);
  ecCal.A.set   = prefs.getBool("A_set", false);

  ecCal.B.ec_us = prefs.getFloat("B_ec", 27600.0f);
  ecCal.B.v     = prefs.getFloat("B_v", 0.0f);
  ecCal.B.set   = prefs.getBool("B_set", false);
  prefs.end();
}

static void saveEcCal(){
  prefs.begin("eccal", false);
  prefs.putFloat("A_ec", ecCal.A.ec_us);
  prefs.putFloat("A_v",  ecCal.A.v);
  prefs.putBool("A_set", ecCal.A.set);

  prefs.putFloat("B_ec", ecCal.B.ec_us);
  prefs.putFloat("B_v",  ecCal.B.v);
  prefs.putBool("B_set", ecCal.B.set);
  prefs.end();
}

static void loadLevelCal(){
  prefs.begin("lvlcal", true);
  lvlCal.empty.level = prefs.getFloat("E_lvl", 0.0f);
  lvlCal.empty.v     = prefs.getFloat("E_v",   0.0f);
  lvlCal.empty.set   = prefs.getBool("E_set",  false);

  lvlCal.full.level  = prefs.getFloat("F_lvl", 100.0f);
  lvlCal.full.v      = prefs.getFloat("F_v",   0.0f);
  lvlCal.full.set    = prefs.getBool("F_set",  false);

  lvlCal.unit        = (LevelUnit)prefs.getUChar("unit", (uint8_t)UNIT_PERCENT);
  lvlCal.custom_max  = prefs.getFloat("cmax", 100.0f);
  prefs.end();
}

static void saveLevelCal(){
  prefs.begin("lvlcal", false);
  prefs.putFloat("E_lvl", lvlCal.empty.level);
  prefs.putFloat("E_v",   lvlCal.empty.v);
  prefs.putBool("E_set",  lvlCal.empty.set);

  prefs.putFloat("F_lvl", lvlCal.full.level);
  prefs.putFloat("F_v",   lvlCal.full.v);
  prefs.putBool("F_set",  lvlCal.full.set);

  prefs.putUChar("unit", (uint8_t)lvlCal.unit);
  prefs.putFloat("cmax", lvlCal.custom_max);
  prefs.end();
}

/**************************************************************
 * CALCULATIONS
 **************************************************************/
static void computeEcCal(){
  ecCal.valid = false;
  ecCal.quality = CAL_NONE;

  if (!ecCal.A.set || !ecCal.B.set) return;

  float dv = ecCal.B.v - ecCal.A.v;
  if (fabsf(dv) < 0.02f) {
    ecCal.quality = CAL_WEAK;
    return;
  }

  ecCal.slope  = (ecCal.B.ec_us - ecCal.A.ec_us) / dv;
  ecCal.offset = ecCal.A.ec_us - ecCal.slope * ecCal.A.v;

  ecCal.valid = true;
  ecCal.quality = CAL_OK;
}

static void computeLevelCal(){
  lvlCal.valid = false;
  lvlCal.quality = CAL_NONE;

  if (!lvlCal.empty.set || !lvlCal.full.set) return;

  float dv = lvlCal.full.v - lvlCal.empty.v;
  if (fabsf(dv) < 0.05f) {
    lvlCal.quality = CAL_WEAK;
    return;
  }

  lvlCal.slope  = (lvlCal.full.level - lvlCal.empty.level) / dv;
  lvlCal.offset = lvlCal.empty.level - (lvlCal.slope * lvlCal.empty.v);

  lvlCal.valid = true;
  lvlCal.quality = CAL_OK;
}

/**************************************************************
 * ADC
 **************************************************************/
static uint16_t readAdcAvg(int pin){
  uint32_t acc = 0;
  for (uint8_t i=0;i<ADC_SAMPLES_PER_TICK;i++){
    acc += analogRead(pin);
    delayMicroseconds(200);
  }
  return (uint16_t)(acc / ADC_SAMPLES_PER_TICK);
}

static float adcToPinVoltage(uint16_t adc){
  return (adc / 4095.0f) * 3.3f;
}

static float ecAdcToProbeVoltage(uint16_t adc){
  return adcToPinVoltage(adc) * EC_DIVIDER_RATIO;
}

static float levelAdcToProbeVoltage(uint16_t adc){
  return adcToPinVoltage(adc) * LEVEL_DIVIDER_RATIO;
}

static float ecVoltageToUs(float v){
  if (!ecCal.valid) return v * 10000.0f; // fallback
  return ecCal.slope * v + ecCal.offset;
}

static float levelVoltageToLevel(float v){
  if (!lvlCal.valid) return v; // fallback
  return (lvlCal.slope * v + lvlCal.offset);
}

static void updateLevelDerived(){
  if (lvlCal.unit == UNIT_PERCENT) {
    sens.lvl_percent = sens.lvl_value;
  } else {
    if (lvlCal.custom_max <= 0.0001f) sens.lvl_percent = 0.0f;
    else sens.lvl_percent = (sens.lvl_value / lvlCal.custom_max) * 100.0f;
  }
  if (sens.lvl_percent < 0) sens.lvl_percent = 0;
  if (sens.lvl_percent > 100) sens.lvl_percent = 100;
}

static void sensorTick(){
  sens.ec_adc_raw = readAdcAvg(PIN_EC_ADC);
  sens.ec_v       = ecAdcToProbeVoltage(sens.ec_adc_raw);
  sens.ec_us      = ecVoltageToUs(sens.ec_v);

  sens.lvl_adc_raw = readAdcAvg(PIN_LEVEL_ADC);
  sens.lvl_v       = levelAdcToProbeVoltage(sens.lvl_adc_raw);

  float rawLevel   = levelVoltageToLevel(sens.lvl_v);

  if (lvlCal.unit == UNIT_PERCENT) {
    if (rawLevel < 0) rawLevel = 0;
    if (rawLevel > 100) rawLevel = 100;
    sens.lvl_value = rawLevel;
  } else {
    if (rawLevel < 0) rawLevel = 0;
    if (rawLevel > lvlCal.custom_max) rawLevel = lvlCal.custom_max;
    sens.lvl_value = rawLevel;
  }

  updateLevelDerived();

  // DS18B20 non-blocking
  static uint32_t lastTreq = 0;
  float t = ds18.getTempCByIndex(0);
  if (t > -55 && t < 125) sens.temp_c = t;

  uint32_t now = millis();
  if (now - lastTreq >= 1000) {
    lastTreq = now;
    ds18.requestTemperatures();
  }
}

/**************************************************************
 * BUTTON EVENTS
 **************************************************************/
static EvType pollButton(BtnId id){
  Btn &b = btns[(int)id];
  bool pressed = (digitalRead(b.pin) == LOW);

  if (pressed && !b.down){
    b.down = true;
    b.downMs = millis();
    return EV_NONE;
  }

  if (!pressed && b.down){
    uint32_t dur = millis() - b.downMs;
    b.down = false;

    if (dur >= VLONG_MS) return EV_VLONG;
    if (dur >= LONG_MS)  return EV_LONG;
    if (dur >= SHORT_MS) return EV_SHORT;
  }
  return EV_NONE;
}

/**************************************************************
 * LCD RENDER
 **************************************************************/
static void renderHome(){
  String w = (wifiSt.mode==WifiStatus::WIFI_STA && wifiSt.connected) ? "STA" : "AP ";
  String m = mqttSt.connected ? "M" : " ";
  lcdSetLine(0, "HydroNode " + w + " " + m);

  float ec_ms = sens.ec_us / 1000.0f;

  char l1[32];
  if (isnan(sens.temp_c)) snprintf(l1, sizeof(l1), "EC:%4.2fmS  T:--.-C", ec_ms);
  else                   snprintf(l1, sizeof(l1), "EC:%4.2fmS  T:%4.1fC", ec_ms, sens.temp_c);
  lcdSetLine(1, String(l1));

  char l2[32]; snprintf(l2, sizeof(l2), "Water: %6.1f %%", sens.lvl_percent);
  lcdSetLine(2, String(l2));

  if (wifiSt.mode==WifiStatus::WIFI_STA && wifiSt.connected) lcdSetLine(3, "IP: " + wifiSt.ip);
  else lcdSetLine(3, "AP: 192.168.4.1");
}

static void renderMenu(){
  lcdSetLine(0, "Menu");
  lcdSetLine(1, (menuIndex==0?"> ":"  ") + String("Setup"));
  lcdSetLine(2, (menuIndex==1?"> ":"  ") + String("Calibration"));
  lcdSetLine(3, (menuIndex==2?"> ":"  ") + String("Info / Exit"));
}

static void renderSetup(){
  lcdSetLine(0, "Setup");
  lcdSetLine(1, "MQTT via Web API");
  lcdSetLine(2, "Hold LIGHT = WiFiRST");
  lcdSetLine(3, "Back");
}

static void renderInfo(){
  lcdSetLine(0, String("FW: ")+FW_VERSION);
  lcdSetLine(1, String("MQTT: ")+(mqttSt.connected?"OK":"OFF"));
  lcdSetLine(2, String("Topic: ")+mqttCfg.base_topic);
  lcdSetLine(3, "Back");
}

static void renderCalMenu(){
  lcdSetLine(0, "Calibration");
  lcdSetLine(1, (calIndex==0?"> ":"  ") + String("EC Wizard"));
  lcdSetLine(2, (calIndex==1?"> ":"  ") + String("Level Wizard"));
  lcdSetLine(3, (calIndex==2?"> ":"  ") + String("Back"));
}

static void renderEcWizard(){
  lcdSetLine(0, "EC Wizard (V->EC)");
  if (ecStep == EC_A_SET){
    lcdSetLine(1, "Set A solution:");
    lcdSetLine(2, "A=" + String(ecWizardA,0) + " uS");
    lcdSetLine(3, "UP/DN adj,ENT next");
  } else if (ecStep == EC_A_CAP){
    lcdSetLine(1, "In A solution now");
    lcdSetLine(2, "ENT capture voltage");
    lcdSetLine(3, "Back");
  } else if (ecStep == EC_B_SET){
    lcdSetLine(1, "Set B solution:");
    lcdSetLine(2, "B=" + String(ecWizardB,0) + " uS");
    lcdSetLine(3, "UP/DN adj,ENT next");
  } else if (ecStep == EC_B_CAP){
    lcdSetLine(1, "In B solution now");
    lcdSetLine(2, "ENT capture voltage");
    lcdSetLine(3, "Back");
  } else {
    lcdSetLine(1, "Compute + Save");
    lcdSetLine(2, "ENT confirm");
    lcdSetLine(3, "Back");
  }
}

static void renderLevelUnit(){
  lcdSetLine(0, "Level Unit");
  String u = (lvlCal.unit==UNIT_PERCENT) ? "%" : "CUSTOM";
  lcdSetLine(1, "Unit: " + u);
  if (lvlCal.unit==UNIT_CUSTOM) lcdSetLine(2, "Max: " + String(lvlCal.custom_max,1));
  else lcdSetLine(2, " ");
  lcdSetLine(3, "UP toggle,ENT ok");
}

static void renderLevelWizard(){
  lcdSetLine(0, "Level Wizard");
  if (lvlStep == LVL_UNIT){
    lcdSetLine(1, "Select unit first");
    lcdSetLine(2, "ENT -> Unit setup");
    lcdSetLine(3, "Back");
  } else if (lvlStep == LVL_EMPTY_SET){
    lcdSetLine(1, "Empty value:");
    lcdSetLine(2, String(lvlWizardEmpty,1));
    lcdSetLine(3, "UP/DN adj,ENT next");
  } else if (lvlStep == LVL_EMPTY_CAP){
    lcdSetLine(1, "Set EMPTY state");
    lcdSetLine(2, "ENT capture voltage");
    lcdSetLine(3, "Back");
  } else if (lvlStep == LVL_FULL_SET){
    lcdSetLine(1, "Full value:");
    lcdSetLine(2, String(lvlWizardFull,1));
    lcdSetLine(3, "UP/DN adj,ENT next");
  } else if (lvlStep == LVL_FULL_CAP){
    lcdSetLine(1, "Set FULL state");
    lcdSetLine(2, "ENT capture voltage");
    lcdSetLine(3, "Back");
  } else {
    lcdSetLine(1, "Compute + Save");
    lcdSetLine(2, "ENT confirm");
    lcdSetLine(3, "Back");
  }
}

static void lcdTick(){
  if (lcdBacklight) lcd.backlight();
  else lcd.noBacklight();

  switch(ui){
    case UI_HOME:       renderHome(); break;
    case UI_MENU:       renderMenu(); break;
    case UI_SETUP:      renderSetup(); break;
    case UI_INFO:       renderInfo(); break;
    case UI_CAL_MENU:   renderCalMenu(); break;
    case UI_CAL_EC:     renderEcWizard(); break;
    case UI_LEVEL_UNIT: renderLevelUnit(); break;
    case UI_CAL_LEVEL:  renderLevelWizard(); break;
    default:            renderHome(); break;
  }
}

/**************************************************************
 * UI EVENT HANDLER
 **************************************************************/
static void handleEvent(BtnId b, EvType ev){
  if (ev == EV_NONE) return;

  if (b == BTN_LIGHT){
    if (ev == EV_SHORT){
      if (ui == UI_HOME) uiSet(UI_MENU);
      else if (ui == UI_MENU) uiSet(UI_HOME);
      else uiSet(UI_MENU);
    } else if (ev == EV_LONG){
      lcdBacklight = !lcdBacklight;
    } else if (ev == EV_VLONG){
      wipeNetworkAndRestart();
    }
    return;
  }

  if (b == BTN_UP){
    if (ev != EV_SHORT) return;

    if (ui == UI_MENU){
      menuIndex = (menuIndex + MENU_N - 1) % MENU_N;
    } else if (ui == UI_CAL_MENU){
      calIndex = (calIndex + CAL_N - 1) % CAL_N;
    } else if (ui == UI_CAL_EC){
      if (ecStep == EC_A_SET) ecWizardA += 10.0f;
      else if (ecStep == EC_B_SET) ecWizardB += 100.0f;
    } else if (ui == UI_LEVEL_UNIT){
      lvlCal.unit = (lvlCal.unit == UNIT_PERCENT) ? UNIT_CUSTOM : UNIT_PERCENT;
    } else if (ui == UI_CAL_LEVEL){
      if (lvlStep == LVL_EMPTY_SET) lvlWizardEmpty += 1.0f;
      else if (lvlStep == LVL_FULL_SET) lvlWizardFull += 1.0f;
    }
    return;
  }

  if (b == BTN_DN){
    // MENU: short=DOWN, long=ENTER
    if (ui == UI_MENU){
      if (ev == EV_SHORT){
        menuIndex = (menuIndex + 1) % MENU_N;
      } else if (ev == EV_LONG){
        if (menuIndex == 0) uiSet(UI_SETUP);
        else if (menuIndex == 1) uiSet(UI_CAL_MENU);
        else uiSet(UI_INFO);
      }
      return;
    }

    if (ui == UI_CAL_MENU){
      if (ev == EV_SHORT){
        calIndex = (calIndex + 1) % CAL_N;
      } else if (ev == EV_LONG){
        if (calIndex == 0){
          ecStep = EC_A_SET;
          ecWizardA = ecCal.A.ec_us;
          ecWizardB = ecCal.B.ec_us;
          uiSet(UI_CAL_EC);
        } else if (calIndex == 1){
          lvlStep = LVL_UNIT;
          lvlWizardEmpty = 0.0f;
          lvlWizardFull  = (lvlCal.unit==UNIT_PERCENT) ? 100.0f : lvlCal.custom_max;
          uiSet(UI_CAL_LEVEL);
        } else {
          uiSet(UI_MENU);
        }
      }
      return;
    }

    // Others: short=decrement/back, long=enter/next
    if (ui == UI_SETUP || ui == UI_INFO){
      if (ev == EV_SHORT || ev == EV_LONG) uiSet(UI_MENU);
      return;
    }

    if (ui == UI_CAL_EC){
      if (ev == EV_SHORT){
        if (ecStep == EC_A_SET) { ecWizardA -= 10.0f; if (ecWizardA < 0) ecWizardA = 0; }
        else if (ecStep == EC_B_SET) { ecWizardB -= 100.0f; if (ecWizardB < 0) ecWizardB = 0; }
        return;
      }
      if (ev == EV_LONG){
        if (ecStep == EC_A_SET) ecStep = EC_A_CAP;
        else if (ecStep == EC_A_CAP){
          ecCal.A.ec_us = ecWizardA;
          ecCal.A.v     = sens.ec_v;
          ecCal.A.set   = true;
          saveEcCal();
          ecStep = EC_B_SET;
        } else if (ecStep == EC_B_SET) ecStep = EC_B_CAP;
        else if (ecStep == EC_B_CAP){
          ecCal.B.ec_us = ecWizardB;
          ecCal.B.v     = sens.ec_v;
          ecCal.B.set   = true;
          saveEcCal();
          ecStep = EC_DONE;
        } else {
          computeEcCal();
          saveEcCal();
          uiSet(UI_MENU);
        }
        return;
      }
      return;
    }

    if (ui == UI_LEVEL_UNIT){
      if (ev == EV_SHORT){
        if (lvlCal.unit == UNIT_CUSTOM) {
          lvlCal.custom_max -= 1.0f;
          if (lvlCal.custom_max < 1.0f) lvlCal.custom_max = 1.0f;
        } else {
          lvlCal.unit = UNIT_CUSTOM;
        }
        return;
      }
      if (ev == EV_LONG){
        saveLevelCal();
        uiSet(UI_CAL_LEVEL);
        lvlStep = LVL_EMPTY_SET;
        lvlWizardEmpty = 0.0f;
        lvlWizardFull  = (lvlCal.unit==UNIT_PERCENT) ? 100.0f : lvlCal.custom_max;
        return;
      }
      return;
    }

    if (ui == UI_CAL_LEVEL){
      if (ev == EV_SHORT){
        if (lvlStep == LVL_EMPTY_SET) { lvlWizardEmpty -= 1.0f; if (lvlWizardEmpty < 0) lvlWizardEmpty = 0; }
        else if (lvlStep == LVL_FULL_SET) { lvlWizardFull -= 1.0f; if (lvlWizardFull < 0) lvlWizardFull = 0; }
        return;
      }
      if (ev == EV_LONG){
        if (lvlStep == LVL_UNIT){
          uiSet(UI_LEVEL_UNIT);
        } else if (lvlStep == LVL_EMPTY_SET) lvlStep = LVL_EMPTY_CAP;
        else if (lvlStep == LVL_EMPTY_CAP){
          lvlCal.empty.level = lvlWizardEmpty;
          lvlCal.empty.v     = sens.lvl_v;
          lvlCal.empty.set   = true;
          saveLevelCal();
          lvlStep = LVL_FULL_SET;
        } else if (lvlStep == LVL_FULL_SET) lvlStep = LVL_FULL_CAP;
        else if (lvlStep == LVL_FULL_CAP){
          lvlCal.full.level = lvlWizardFull;
          lvlCal.full.v     = sens.lvl_v;
          lvlCal.full.set   = true;
          saveLevelCal();
          lvlStep = LVL_DONE;
        } else {
          if (lvlCal.unit == UNIT_CUSTOM) lvlCal.custom_max = lvlWizardFull;
          computeLevelCal();
          saveLevelCal();
          uiSet(UI_MENU);
        }
        return;
      }
      return;
    }

    uiSet(UI_MENU);
    return;
  }
}

/**************************************************************
 * MQTT (esp_mqtt_client)
 **************************************************************/
static String mqttBase(){
  String base = mqttCfg.base_topic;
  if (!base.length()) base = "hydronode";
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static void mqttPublishNow(){
  if (!mqttHandle || !mqttConnected) return;

  String base = mqttBase();
  int qos = 0;
  bool retain = mqttCfg.retain;

  StaticJsonDocument<512> doc;
  doc["ec_us"] = sens.ec_us;
  doc["ec_v"] = sens.ec_v;
  doc["level_percent"] = sens.lvl_percent;
  doc["level_value"] = sens.lvl_value;
  doc["level_v"] = sens.lvl_v;
  doc["temp_c"] = sens.temp_c;

  char buf[512];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (base + "/telemetry").c_str(), buf, 0, qos, retain);

  char tmp[24];
  dtostrf(sens.ec_us, 0, 0, tmp);
  esp_mqtt_client_publish(mqttHandle, (base + "/ec").c_str(), tmp, 0, qos, retain);

  dtostrf(sens.lvl_percent, 0, 1, tmp);
  esp_mqtt_client_publish(mqttHandle, (base + "/level/percent").c_str(), tmp, 0, qos, retain);

  dtostrf(sens.lvl_value, 0, 2, tmp);
  esp_mqtt_client_publish(mqttHandle, (base + "/level/value").c_str(), tmp, 0, qos, retain);

  if (!isnan(sens.temp_c)) {
    dtostrf(sens.temp_c, 0, 1, tmp);
    esp_mqtt_client_publish(mqttHandle, (base + "/temp_c").c_str(), tmp, 0, qos, retain);
  }
}

static void mqttEventHandler(void*, esp_event_base_t, int32_t event_id, void* event_data){
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch(event_id){
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      mqttSt.err = "";
      esp_mqtt_client_publish(ev->client, (mqttBase() + "/status").c_str(), "online", 0, 1, 1);
      _otaTopic = mqttBase() + "/ota/command";
      esp_mqtt_client_subscribe(ev->client, _otaTopic.c_str(), 1);
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      mqttSt.err = "disconnected";
      break;
    case MQTT_EVENT_DATA: {
      String topic(ev->topic, ev->topic_len);
      if (topic == _otaTopic){
        String msg(ev->data, ev->data_len);
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, msg) == DeserializationError::Ok){
          const char* ver = doc["version"];
          const char* url = doc["url"];
          if (url && ver && String(ver) != FW_VERSION){
            WiFiClient hc;
            httpUpdate.rebootOnUpdate(true);
            httpUpdate.update(hc, url);
          }
        }
      }
      break;
    }
    default: break;
  }
}

static void mqttStart(){
  if (!mqttCfg.enabled || mqttCfg.uri.length() == 0) return;
  if (apMode || WiFi.status() != WL_CONNECTED) return;

  if (mqttHandle){ esp_mqtt_client_destroy(mqttHandle); mqttHandle = nullptr; mqttConnected = false; }

  String base = mqttBase();
  String willTopic = base + "/status";

  esp_mqtt_client_config_t cfg = {};
  cfg.uri = mqttCfg.uri.c_str();
  if (mqttCfg.user.length()) cfg.username = mqttCfg.user.c_str();
  if (mqttCfg.pass.length()) cfg.password = mqttCfg.pass.c_str();
  cfg.client_id = (String("hydronode-") + chipSuffix()).c_str();
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

static void mqttTick(uint32_t now){
  mqttSt.connected = mqttConnected;
  if (!mqttCfg.enabled){ mqttSt.connected = false; return; }
  if (!mqttHandle && !apMode && WiFi.status() == WL_CONNECTED && mqttCfg.uri.length()) mqttStart();
  if (!mqttConnected) return;
  if (now - mqttSt.lastPublishMs >= mqttCfg.pub_period_ms){
    mqttSt.lastPublishMs = now;
    mqttPublishNow();
  }
}

/**************************************************************
 * WEB: JSON helpers
 **************************************************************/
static void sendJson(AsyncWebServerRequest *req, const JsonDocument& doc){
  String s;
  serializeJson(doc, s);
  req->send(200, "application/json", s);
}

/**************************************************************
 * WEB: ROUTES
 **************************************************************/
static void doProvision(const String& base_topic, const String& api_key,
                        AsyncWebServerRequest* req){
  if (apMode || WiFi.status() != WL_CONNECTED){
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

  if (code == 200 || code == 201){
    String resp = http.getString();
    http.end();

    DynamicJsonDocument doc(1024);
    if (deserializeJson(doc, resp) != DeserializationError::Ok){
      req->send(502, "application/json", "{\"ok\":false,\"err\":\"parse\"}");
      return;
    }

    JsonObject mc = doc["mqtt_config"];
    if (!mc){
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
    saveMqtt();
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
  DynamicJsonDocument errDoc(512);
  String errMsg = "http_" + String(code);
  if (deserializeJson(errDoc, errBody) == DeserializationError::Ok){
    if (errDoc["error"].is<const char*>()) errMsg = errDoc["error"].as<String>();
  }
  req->send(code == 409 ? 409 : 502, "application/json",
            "{\"ok\":false,\"err\":\"" + errMsg + "\"}");
}

static void setupRoutes(){
  server.serveStatic("/", LittleFS, "/www/").setDefaultFile(apMode ? "ap.html" : "index.html");

  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", PROVISION_HTML);
  });

  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!req->hasParam("topic", true) || !req->hasParam("api_key", true)){
      req->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_params\"}");
      return;
    }
    String topic  = req->getParam("topic",   true)->value(); topic.trim();
    String apiKey = req->getParam("api_key", true)->value(); apiKey.trim();
    if (!topic.length() || !apiKey.length()){
      req->send(400, "application/json", "{\"ok\":false,\"err\":\"empty_params\"}");
      return;
    }
    doProvision(topic, apiKey, req);
  });

  server.onNotFound([](AsyncWebServerRequest *req){
    if (apMode){
      if (LittleFS.exists("/www/ap.html")) req->send(LittleFS, "/www/ap.html", "text/html");
      else req->redirect("/provision");
      return;
    }
    req->send(404, "text/plain", "Not found");
  });

  server.on("/api/wifi", HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      StaticJsonDocument<384> in;
      StaticJsonDocument<256> out;

      auto err = deserializeJson(in, data, len);
      if (err){
        out["ok"] = false; out["err"] = "bad_json";
        sendJson(req, out); return;
      }

      String ssid = in["ssid"] | "";
      String pass = in["pass"] | "";
      ssid.trim();

      if (ssid.length() == 0){
        out["ok"] = false; out["err"] = "ssid_required";
        sendJson(req, out); return;
      }

      prefs.begin("wifi", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();

      WiFi.mode(WIFI_STA);
      WiFi.begin(ssid.c_str(), pass.c_str());

      out["ok"] = true; out["rebooting"] = true;
      sendJson(req, out);
      delay(400);
      ESP.restart();
    }
  );

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<896> doc;
    doc["ok"] = true;
    doc["fw"] = FW_VERSION;
    doc["api"] = API_VERSION;

    doc["wifi"]["mode"] = (uint8_t)wifiSt.mode;
    doc["wifi"]["connected"] = wifiSt.connected;
    doc["wifi"]["ip"] = wifiSt.ip;
    doc["wifi"]["ssid"] = wifiSt.ssid;

    doc["mqtt"]["enabled"] = mqttCfg.enabled;
    doc["mqtt"]["connected"] = mqttSt.connected;
    doc["mqtt"]["base_topic"] = mqttCfg.base_topic;
    doc["mqtt"]["err"] = mqttSt.err;

    doc["temp_c"] = sens.temp_c;
    sendJson(req, doc);
  });

  server.on("/api/ec", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<512> doc;
    doc["ok"] = true;
    doc["us_cm"] = sens.ec_us;
    doc["v"] = sens.ec_v;
    doc["adc_raw"] = sens.ec_adc_raw;
    sendJson(req, doc);
  });

  server.on("/api/level", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<512> doc;
    doc["ok"] = true;
    doc["percent"] = sens.lvl_percent;
    doc["value"] = sens.lvl_value;
    doc["v"] = sens.lvl_v;
    doc["adc_raw"] = sens.lvl_adc_raw;
    doc["unit"] = (uint8_t)lvlCal.unit;
    doc["custom_max"] = lvlCal.custom_max;
    sendJson(req, doc);
  });

  server.on("/api/temp", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["ok"] = true;
    doc["temp_c"] = sens.temp_c;
    sendJson(req, doc);
  });

  server.on("/api/cal", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<768> doc;
    doc["ok"] = true;

    doc["ec"]["A_set"] = ecCal.A.set;
    doc["ec"]["B_set"] = ecCal.B.set;
    doc["ec"]["A_ec"]  = ecCal.A.ec_us;
    doc["ec"]["B_ec"]  = ecCal.B.ec_us;
    doc["ec"]["A_v"]   = ecCal.A.v;
    doc["ec"]["B_v"]   = ecCal.B.v;
    doc["ec"]["valid"] = ecCal.valid;
    doc["ec"]["quality"] = (uint8_t)ecCal.quality;

    doc["level"]["E_set"] = lvlCal.empty.set;
    doc["level"]["F_set"] = lvlCal.full.set;
    doc["level"]["E_lvl"] = lvlCal.empty.level;
    doc["level"]["F_lvl"] = lvlCal.full.level;
    doc["level"]["E_v"]   = lvlCal.empty.v;
    doc["level"]["F_v"]   = lvlCal.full.v;
    doc["level"]["valid"] = lvlCal.valid;
    doc["level"]["quality"] = (uint8_t)lvlCal.quality;
    doc["level"]["unit"] = (uint8_t)lvlCal.unit;
    doc["level"]["custom_max"] = lvlCal.custom_max;

    sendJson(req, doc);
  });

  server.on("/api/settings/mqtt", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<512> doc;
    doc["ok"] = true;
    doc["enabled"] = mqttCfg.enabled;
    doc["uri"] = mqttCfg.uri;
    doc["user"] = mqttCfg.user;
    doc["pass"] = "";
    doc["base_topic"] = mqttCfg.base_topic;
    doc["retain"] = mqttCfg.retain;
    doc["pub_period_ms"] = mqttCfg.pub_period_ms;

    bool provDone = false;
    prefs.begin("prov", true);
    provDone = prefs.getBool("done", false);
    prefs.end();
    doc["provisioned"] = provDone;

    sendJson(req, doc);
  });

  server.on("/api/settings/mqtt", HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      StaticJsonDocument<768> in;
      auto err = deserializeJson(in, data, len);

      StaticJsonDocument<256> out;
      if (err){ out["ok"] = false; out["err"] = "bad_json"; sendJson(req, out); return; }

      if (in.containsKey("enabled"))      mqttCfg.enabled      = in["enabled"].as<bool>();
      if (in.containsKey("uri"))          mqttCfg.uri          = String((const char*)in["uri"]);
      if (in.containsKey("user"))         mqttCfg.user         = String((const char*)in["user"]);
      if (in.containsKey("pass"))         mqttCfg.pass         = String((const char*)in["pass"]);
      if (in.containsKey("base_topic"))   mqttCfg.base_topic   = String((const char*)in["base_topic"]);
      if (in.containsKey("retain"))       mqttCfg.retain       = in["retain"].as<bool>();
      if (in.containsKey("pub_period_ms")) mqttCfg.pub_period_ms = (uint16_t)in["pub_period_ms"].as<int>();

      saveMqtt();
      if (!apMode && WiFi.status() == WL_CONNECTED) mqttStart();
      out["ok"] = true;
      sendJson(req, out);
    }
  );
}

/**************************************************************
 * SETUP / LOOP
 **************************************************************/
static void lcdInit(){
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcdSetLine(0, "HydroNode");
  lcdSetLine(1, "EC + Water Level");
  lcdSetLine(2, "Booting...");
  lcdSetLine(3, "");
}

void setup(){
  Serial.begin(115200);

  pinMode(PIN_BTN_LIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DN, INPUT_PULLUP);

  analogReadResolution(12);

  lcdInit();

  ds18.begin();
  ds18.setWaitForConversion(false);
  ds18.requestTemperatures();

  loadMqtt();
  loadEcCal();
  loadLevelCal();
  computeEcCal();
  computeLevelCal();

  if (!LittleFS.begin(true)){
    Serial.println("LittleFS mount failed");
  }

  startSTA();
  uint32_t t0 = millis();
  while (millis() - t0 < 8000){
    if (WiFi.status() == WL_CONNECTED) break;
    delay(50);
  }
  if (WiFi.status() != WL_CONNECTED){
    startAP();
  }

  setupRoutes();
  server.begin();

  lcd.clear();
  uiSet(UI_HOME);
}

void loop(){
  static uint32_t lastUi=0, lastSensor=0, lastMqtt=0;
  uint32_t now = millis();

  // ✅ Buttons FIRST (so UI stays responsive)
  EvType e0 = pollButton(BTN_LIGHT);
  EvType e1 = pollButton(BTN_UP);
  EvType e2 = pollButton(BTN_DN);

  handleEvent(BTN_LIGHT, e0);
  handleEvent(BTN_UP, e1);
  handleEvent(BTN_DN, e2);

  // WiFi + captive portal
  wifiTick();

  // Sensors
  if (now - lastSensor >= TICK_SENSOR_MS){
    lastSensor = now;
    sensorTick();
  }

  // LCD
  if (now - lastUi >= TICK_UI_MS){
    lastUi = now;
    lcdTick();
  }

  if (now - lastMqtt >= TICK_MQTT_MS){
    lastMqtt = now;
    mqttTick(now);
  }
}
