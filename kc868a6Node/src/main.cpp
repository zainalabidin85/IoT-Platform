/**************************************************************
 * KinCony KC868-A6 — 6-Relay Controller
 *
 * Basic Auth (STA mode):
 *   USER: admin  PASS: kc868a6
 *
 * Hardware (via I2C expanders):
 *   I2C SDA: GPIO4   SCL: GPIO15
 *   Relay board (PCF8574 @ 0x24): R1–R6, ACTIVE LOW
 *   Input board (PCF8574 @ 0x22): DI1–DI6, ACTIVE LOW
 *
 * Analog / 1-Wire sensors:
 *   GPIO36 (A1): EC sensor, 0–5V
 *   GPIO39 (A2): Water level sensor, 0–5V
 *   GPIO32 (1W1): DS18B20 temperature sensor
 *
 * MQTT topics (base topic example: a6/a6-001):
 *   Relay command : <base>/relay/<1-6>/set   payload: ON|OFF|1|0|TOGGLE
 *   Relay state   : <base>/relay/<1-6>/state (retained)
 *   Input state   : <base>/input/<1-6>/state (retained)
 *   Telemetry     : <base>/telemetry
 *   Availability  : <base>/status            payload: online|offline
 **************************************************************/

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "mqtt_client.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include "esp_wifi.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

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

// -------------------- I2C / Expander --------------------
#define I2C_SDA         4
#define I2C_SCL         15
#define I2C_RELAY_ADDR  0x24   // PCF8574 relay board, active low
#define I2C_INPUT_ADDR  0x22   // PCF8574 input board, active low

// -------------------- OLED (SSD1306 128x64) --------------------
#define OLED_ADDR    0x3C
#define OLED_WIDTH   128
#define OLED_HEIGHT  64
static Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool            oledOk    = false;
static volatile bool   oledDirty = true;
static uint8_t         oledPage  = 0;     // 0=relay/input  1=sensors
static uint32_t        oledPageAt = 0;
static const uint32_t  OLED_PAGE_MS = 4000;

#define NUM_RELAYS  6
#define NUM_INPUTS  6

// -------------------- Sensor pins --------------------
#define PIN_EC_ADC    36   // ANALOG_A1 (0-5V)
#define PIN_WATER_ADC 39   // ANALOG_A2 (0-5V)
#define PIN_ONEWIRE   32   // 1-Wire Bus 1 (DS18B20)

static OneWire          oneWire(PIN_ONEWIRE);
static DallasTemperature tempSensor(&oneWire);

// Sensor readings (NaN = not yet read)
static float ecVoltage    = NAN;
static float waterPercent = NAN;
static float tempC        = NAN;

// EC two-point calibration: EC (mS/cm) = (ecVoltage - ecOffset) * ecSlope
// Calibrate by measuring probe voltage in two known standard solutions.
static float ecSlope  = 1.0f;
static float ecOffset = 0.0f;

// Water level calibration: voltage at empty tank and full tank
static float waterVMin = 0.0f;   // volts when tank is empty (0%)
static float waterVMax = 5.0f;   // volts when tank is full (100%)

// -------------------- FS/DNS --------------------
static const char* FS_ROOT  = "/www";
static const byte  DNS_PORT = 53;

// -------------------- Debounce --------------------
static const uint32_t INPUT_DEBOUNCE_MS = 50;

// -------------------- Web/MQTT --------------------
AsyncWebServer server(80);
DNSServer      dns;

static esp_mqtt_client_handle_t mqttHandle    = nullptr;
static volatile bool             mqttConnected = false;
Preferences prefs;

static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "kc868a6Node";

// -------------------- Basic Auth --------------------
static const bool  BASIC_AUTH_ON = true;
static const char* BASIC_USER    = "admin";
static const char* BASIC_PASS    = "kc868a6";

// -------------------- State --------------------
bool    relayState[NUM_RELAYS] = {};
uint8_t relayMask = 0xFF;   // all OFF (active low: 1=OFF, 0=ON)

struct DebouncedInput {
  int      last_read;
  int      stable;
  uint32_t last_change_ms;
} inputs[NUM_INPUTS];

// -------------------- Mode --------------------
enum Mode { MODE_AP, MODE_STA };
Mode modeNow = MODE_AP;

static uint32_t wifiDropAt = 0;  // millis() when WiFi first lost; 0 = connected
static bool     wifiWasUp  = false;

// -------------------- IDs --------------------
String deviceId;
String shortId;
String mdnsHost;
String mdnsFqdn;

// -------------------- WiFi config --------------------
struct WifiCfg { String ssid, pass; } wifiCfg;

// -------------------- MQTT config --------------------
struct MqttCfg {
  bool   enabled = false;
  String uri, user, pass, cmdTopic, stateTopic;
} mqttCfg;

String baseTopic;
String tAvail;
String tRelaySetWild;
String tRelayStatePrefix;
String tInputStatePrefix;

// -------------------- I2C helpers --------------------
static void i2cWriteRelays() {
  Wire.beginTransmission(I2C_RELAY_ADDR);
  Wire.write(relayMask);
  Wire.endTransmission();
}

static uint8_t i2cReadInputs() {
  Wire.requestFrom((uint8_t)I2C_INPUT_ADDR, (uint8_t)1);
  if (Wire.available()) return Wire.read();
  return 0xFF;  // all open on read failure
}

// -------------------- Helpers --------------------
static String macToDeviceId() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[32];
  snprintf(buf, sizeof(buf), "esp32-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

static String macSuffix6() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

static void applyTopics() {
  baseTopic = mqttCfg.cmdTopic;
  baseTopic.trim();
  while (baseTopic.endsWith("/")) baseTopic.remove(baseTopic.length() - 1);

  tAvail            = baseTopic + "/status";
  tRelaySetWild     = baseTopic + "/relay/+/set";
  tRelayStatePrefix = baseTopic + "/relay/";
  tInputStatePrefix = baseTopic + "/input/";
}

// -------------------- Debug WiFi --------------------
static const char* wlStatusStr(wl_status_t st) {
  switch (st) {
    case WL_NO_SHIELD:       return "WL_NO_SHIELD";
    case WL_IDLE_STATUS:     return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:   return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:  return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:       return "WL_CONNECTED";
    case WL_CONNECT_FAILED:  return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:    return "WL_DISCONNECTED";
    default: return "WL_UNKNOWN";
  }
}

static const char* wifiDiscReasonStr(int reason) {
  switch (reason) {
    case 1:  return "UNSPECIFIED";
    case 2:  return "AUTH_EXPIRE";
    case 3:  return "AUTH_LEAVE";
    case 8:  return "ASSOC_LEAVE";
    case 15: return "4WAY_HANDSHAKE_TIMEOUT";
    case 201:return "NO_AP_FOUND";
    case 202:return "AUTH_FAIL";
    case 203:return "ASSOC_FAIL";
    case 204:return "HANDSHAKE_TIMEOUT";
    default: return "UNKNOWN";
  }
}

static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] STA_CONNECTED");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFi] GOT_IP: %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFi] DISCONNECTED reason=%d (%s)\n",
                    (int)info.wifi_sta_disconnected.reason,
                    wifiDiscReasonStr((int)info.wifi_sta_disconnected.reason));
      break;
    default: break;
  }
}

// -------------------- Auth --------------------
static inline bool authOK(AsyncWebServerRequest* r) {
  if (!BASIC_AUTH_ON) return true;
  return r->authenticate(BASIC_USER, BASIC_PASS);
}
static inline bool requireAuthOr401(AsyncWebServerRequest* r) {
  if (authOK(r)) return true;
  r->requestAuthentication();
  return false;
}

// -------------------- MQTT publish --------------------
static void mqttPublishRetained(const String& topic, const char* payload) {
  if (!mqttConnected) return;
  esp_mqtt_client_publish(mqttHandle, topic.c_str(), payload, 0, 1, 1);
}

static void publishAvailability(bool online) {
  if (!mqttConnected || !tAvail.length()) return;
  mqttPublishRetained(tAvail, online ? "online" : "offline");
}

static void publishRelayStateOne(int idx) {
  if (!mqttConnected || !baseTopic.length()) return;
  String t = tRelayStatePrefix + String(idx + 1) + "/state";
  mqttPublishRetained(t, relayState[idx] ? "ON" : "OFF");
}

static void publishAllRelayStates() {
  for (int i = 0; i < NUM_RELAYS; i++) publishRelayStateOne(i);
}

static void publishInputStateOne(int idx) {
  if (!mqttConnected || !baseTopic.length()) return;
  String t = tInputStatePrefix + String(idx + 1) + "/state";
  mqttPublishRetained(t, (inputs[idx].stable == LOW) ? "ON" : "OFF");
}

static void publishAllInputStates() {
  for (int i = 0; i < NUM_INPUTS; i++) publishInputStateOne(i);
}

static void publishTelemetry() {
  if (!mqttConnected || !baseTopic.length()) return;
  StaticJsonDocument<512> doc;
  JsonArray relays = doc.createNestedArray("relay");
  for (int i = 0; i < NUM_RELAYS; i++) relays.add(relayState[i] ? 1 : 0);
  JsonArray ins = doc.createNestedArray("input");
  for (int i = 0; i < NUM_INPUTS; i++) ins.add((inputs[i].stable == LOW) ? 1 : 0);
  if (!isnan(tempC))        doc["temperature"]  = serialized(String(tempC, 1));
  if (!isnan(ecVoltage))    doc["ec"]           = serialized(String((ecVoltage - ecOffset) * ecSlope, 2));
  if (!isnan(waterPercent)) doc["water_level"]  = serialized(String(waterPercent, 1));
  doc["ts_ms"] = millis();
  char buf[512];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (baseTopic + "/telemetry").c_str(), buf, 0, 0, 0);
}

// -------------------- OLED --------------------
static void updateOLED() {
  if (!oledOk) return;
  oledDirty = false;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Row 0: title + mode
  display.setCursor(0, 0);
  display.print("KC868-A6");
  const char* modeTag = (modeNow == MODE_STA) ? "[STA]" : "[AP] ";
  display.setCursor(OLED_WIDTH - 30, 0);
  display.print(modeTag);

  // Row 1: IP address
  display.setCursor(0, 10);
  if (modeNow == MODE_STA && WiFi.status() == WL_CONNECTED) {
    display.print(WiFi.localIP().toString());
  } else if (modeNow == MODE_AP) {
    display.print(WiFi.softAPIP().toString());
  } else {
    display.print("Connecting...");
  }

  // Row 2: MQTT status
  display.setCursor(0, 20);
  display.print("MQTT: ");
  if (!mqttCfg.enabled)   display.print("Not configured");
  else if (mqttConnected) display.print("Connected");
  else                    display.print("Disconnected");

  // Separator line
  display.drawFastHLine(0, 30, OLED_WIDTH, SSD1306_WHITE);

  if (oledPage == 0) {
    // Page 0 — Relay & Input states
    display.setCursor(0, 33);
    display.print("R: ");
    for (int i = 0; i < NUM_RELAYS; i++)
      display.print(relayState[i] ? '#' : '-');

    display.setCursor(0, 43);
    display.print("I: ");
    for (int i = 0; i < NUM_INPUTS; i++)
      display.print((inputs[i].stable == LOW) ? '*' : '.');

    display.setCursor(0, 54);
    display.print(mdnsHost);
  } else {
    // Page 1 — Sensor readings
    char buf[22];

    display.setCursor(0, 33);
    if (isnan(tempC)) display.print("Temp: --.-  C");
    else { snprintf(buf, sizeof(buf), "Temp: %.1f C", tempC); display.print(buf); }

    display.setCursor(0, 43);
    if (isnan(ecVoltage)) display.print("EC:   -.- mS/cm");
    else { snprintf(buf, sizeof(buf), "EC:   %.2f mS/cm", (ecVoltage - ecOffset) * ecSlope); display.print(buf); }

    display.setCursor(0, 54);
    if (isnan(waterPercent)) display.print("Water: ---%");
    else { snprintf(buf, sizeof(buf), "Water: %.1f%%", waterPercent); display.print(buf); }
  }

  display.display();
}

// -------------------- Relay control --------------------
static void setRelay(int idx, bool on) {
  if (idx < 0 || idx >= NUM_RELAYS) return;
  relayState[idx] = on;
  // PCF8574 active low: ON = bit clear, OFF = bit set
  if (on) relayMask &= ~(1 << idx);
  else    relayMask |=  (1 << idx);
  i2cWriteRelays();
  Serial.printf("[RELAY %d] %s (mask=0x%02X)\n", idx + 1, on ? "ON" : "OFF", relayMask);
  oledDirty = true;
  publishRelayStateOne(idx);
  publishTelemetry();
}

static void toggleRelay(int idx) {
  setRelay(idx, !relayState[idx]);
}

// -------------------- Preferences --------------------
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
  mqttCfg.enabled    = prefs.getBool("en", false);
  mqttCfg.uri        = prefs.getString("uri", "");
  mqttCfg.user       = prefs.getString("user", "");
  mqttCfg.pass       = prefs.getString("pass", "");
  mqttCfg.cmdTopic   = prefs.getString("cmd", "");
  mqttCfg.stateTopic = prefs.getString("st", "");
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
  prefs.putString("st",   mqttCfg.stateTopic);
  prefs.end();
}

// -------------------- WiFi --------------------
static bool connectSTA(uint32_t timeoutMs = 20000) {
  if (!wifiCfg.ssid.length()) { Serial.println("[WiFi] No SSID."); return false; }
  Serial.printf("[WiFi] Connecting to [%s]...\n", wifiCfg.ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(mdnsHost.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());
  wl_status_t last = WL_IDLE_STATUS;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st != last) { last = st; Serial.printf("[WiFi] status=%s\n", wlStatusStr(st)); }
    if (st == WL_CONNECTED) {
      Serial.printf("[WiFi] IP=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      return true;
    }
    delay(250);
  }
  return false;
}

static void startAPPortal() {
  WiFi.disconnect(true, true);
  delay(200);
  const String apSsid = "KC868-A6-" + shortId;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), nullptr);
  delay(200);
  const IPAddress ip = WiFi.softAPIP();
  dns.start(DNS_PORT, "*", ip);
  Serial.println("[AP] SSID: " + apSsid + "  IP: " + ip.toString());
}

// -------------------- mDNS --------------------
static void startMDNS() {
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] http://" + mdnsFqdn + "/");
  }
}

// -------------------- MQTT --------------------
static bool mqttReady() {
  return mqttCfg.enabled && mqttCfg.uri.length() && baseTopic.length();
}

static bool parseOnOffToggle(const String& s, bool& outOn, bool& isToggle) {
  String v = s; v.trim(); v.toUpperCase();
  isToggle = false;
  if (v == "TOGGLE")                          { isToggle = true; return true; }
  if (v == "ON"  || v == "1" || v == "TRUE")  { outOn = true;  return true; }
  if (v == "OFF" || v == "0" || v == "FALSE") { outOn = false; return true; }
  return false;
}

static bool handleRelaySetTopic(const String& topic, const String& payload) {
  const String prefix = baseTopic + "/relay/";
  if (!topic.startsWith(prefix)) return false;
  String rest = topic.substring(prefix.length());
  int slash = rest.indexOf('/');
  if (slash < 0) return false;
  if (rest.substring(slash + 1) != "set") return false;
  int n = rest.substring(0, slash).toInt();
  if (n < 1 || n > NUM_RELAYS) return false;
  bool on = false, isToggle = false;
  if (!parseOnOffToggle(payload, on, isToggle)) return true;
  if (isToggle) toggleRelay(n - 1);
  else          setRelay(n - 1, on);
  return true;
}

static void mqttOnMessage(const String& topic, const String& msg) {
  Serial.printf("[MQTT] RX %s = %s\n", topic.c_str(), msg.c_str());
  if (handleRelaySetTopic(topic, msg)) return;
  Serial.println("[MQTT] Unhandled topic");
}

static void mqttEventHandler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      oledDirty = true;
      Serial.println("[MQTT] Connected.");
      esp_mqtt_client_subscribe(mqttHandle, tRelaySetWild.c_str(), 1);
      publishAvailability(true);
      publishAllRelayStates();
      publishAllInputStates();
      publishTelemetry();
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      oledDirty = true;
      Serial.println("[MQTT] Disconnected.");
      break;
    case MQTT_EVENT_DATA:
      if (ev->topic && ev->data)
        mqttOnMessage(String(ev->topic, ev->topic_len),
                      String(ev->data,  ev->data_len));
      break;
    default: break;
  }
}

static void mqttStart() {
  if (!mqttReady()) return;
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
  cfg.lwt_topic            = tAvail.c_str();
  cfg.lwt_msg              = "offline";
  cfg.lwt_msg_len          = 7;
  cfg.lwt_qos              = 1;
  cfg.lwt_retain           = 1;
  cfg.reconnect_timeout_ms = 5000;
  cfg.cert_pem             = ISRG_ROOT_X1;
  Serial.printf("[MQTT] uri=%s base=%s\n", mqttCfg.uri.c_str(), baseTopic.c_str());
  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                  mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

// -------------------- Platform provisioning --------------------
static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>KC868-A6 - Connect to Platform</title>
<style>
body{font-family:system-ui,Arial;margin:24px}
.card{max-width:520px;padding:16px;border:1px solid #ddd;border-radius:12px}
input{width:100%;padding:10px;margin:8px 0;border:1px solid #ccc;border-radius:10px;box-sizing:border-box}
button{width:100%;padding:12px;border:0;border-radius:10px;background:#2a8cff;color:white;font-weight:700;cursor:pointer}
button:disabled{background:#aaa}
#msg{margin-top:12px;padding:10px;border-radius:8px;display:none}
.ok{background:#d4edda;color:#155724;display:block}
.err{background:#f8d7da;color:#721c24;display:block}
</style></head><body>
<div class="card">
<h2>Connect to Platform</h2>
<p><small>Enter your topic and API key from iot.unitani.com.</small></p>
<label>Topic (e.g. a6/a6-001)</label>
<input id="topic" placeholder="a6/a6-001" required>
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
  if (!topic || !apikey) { show('Topic and API key are required.', false); return; }
  btn.disabled = true; btn.textContent = 'Connecting...';
  document.getElementById('msg').style.display = 'none';
  try {
    const fd = new FormData();
    fd.append('topic', topic);
    fd.append('api_key', apikey);
    const r = await fetch('/api/provision', { method: 'POST', body: fd });
    const j = await r.json();
    if (j.ok) { show('Provisioned! MQTT configured.', true); btn.textContent = 'Done'; }
    else { show('Error: ' + (j.error || 'unknown'), false); btn.disabled = false; btn.textContent = 'Connect'; }
  } catch(e) { show('Request failed: ' + e, false); btn.disabled = false; btn.textContent = 'Connect'; }
}
function show(text, ok) {
  const el = document.getElementById('msg');
  el.textContent = text;
  el.className = ok ? 'ok' : 'err';
}
</script>
</body></html>
)HTML";

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
        mqttCfg.enabled    = true;
        mqttCfg.uri        = cfg["uri"]   | "";
        mqttCfg.user       = cfg["user"]  | "";
        mqttCfg.pass       = cfg["pass"]  | "";
        mqttCfg.cmdTopic   = cfg["topic"] | topic.c_str();
        mqttCfg.stateTopic = "";
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
  req->send(code == 409 ? 409 : 502, "application/json",
            "{\"ok\":false,\"error\":\"" + errMsg + "\"}");
}

// -------------------- Web routes (AP) --------------------
static void setupRoutes_AP() {
  const char* redirects[] = {
    "/connecttest.txt","/ncc.txt","/generate_204","/hotspot-detect.html",
    "/fwlink","/canonical.html","/success.txt",
    "/library/test/success.html","/redirect","/ncsi.txt"
  };
  for (auto path : redirects)
    server.on(path, HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/style.css", "text/css");
  });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/app.js", "application/javascript");
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    String json = "{\"ok\":true,\"mode\":\"ap\",\"mdns\":\"" + mdnsFqdn + "\"}";
    r->send(200, "application/json", json);
  });
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *r){
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"encryption\":\"" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURE") + "\"}";
    }
    json += "]}";
    r->send(200, "application/json", json);
    WiFi.scanDelete();
  });
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *r){
    auto v = [&](const char* k) -> String {
      return r->hasParam(k, true) ? r->getParam(k, true)->value() : "";
    };
    String ssid = v("ssid");
    if (!ssid.length()) { r->send(400, "application/json", "{\"ok\":false,\"err\":\"ssid_required\"}"); return; }
    wifiCfg.ssid = ssid;
    wifiCfg.pass = v("pass");
    saveWifiCfg();
    r->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    delay(500);
    ESP.restart();
  });
  server.onNotFound([](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.begin();
  Serial.println("[AP] Web server started.");
}

// -------------------- Web routes (STA) --------------------
static void setupRoutes_STA() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send(LittleFS, "/www/index.html", "text/html");
  });
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send(LittleFS, "/www/settings.html", "text/html");
  });
  {
    auto& h = server.serveStatic("/", LittleFS, FS_ROOT);
    h.setFilter([](AsyncWebServerRequest *r){ return authOK(r); });
  }
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send_P(200, "text/html", PROVISION_HTML);
  });
  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    if (!r->hasParam("topic", true) || !r->hasParam("api_key", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_params\"}"); return;
    }
    String topic  = r->getParam("topic",   true)->value(); topic.trim();
    String apiKey = r->getParam("api_key", true)->value(); apiKey.trim();
    if (!topic.length() || !apiKey.length()) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"empty_params\"}"); return;
    }
    doProvision(topic, apiKey, r);
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    StaticJsonDocument<512> d;
    d["ok"]             = true;
    d["mode"]           = "sta";
    d["ip"]             = WiFi.localIP().toString();
    d["mdns"]           = mdnsFqdn;
    d["rssi"]           = WiFi.RSSI();
    d["mqtt_enabled"]   = mqttCfg.enabled;
    d["mqtt_connected"] = mqttConnected;
    d["mqtt_base"]      = baseTopic;
    JsonArray rArr = d.createNestedArray("relays");
    for (int i = 0; i < NUM_RELAYS; i++) rArr.add(relayState[i]);
    JsonArray iArr = d.createNestedArray("inputs_closed");
    for (int i = 0; i < NUM_INPUTS; i++) iArr.add(inputs[i].stable == LOW);
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  server.on("/api/relay", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    if (!r->hasParam("relay", true) || !r->hasParam("state", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_params\"}"); return;
    }
    int idx = r->getParam("relay", true)->value().toInt() - 1;
    if (idx < 0 || idx >= NUM_RELAYS) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"invalid_relay\"}"); return;
    }
    const String s = r->getParam("state", true)->value();
    bool on = (s == "1" || s.equalsIgnoreCase("on") || s.equalsIgnoreCase("true"));
    setRelay(idx, on);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // Sensor readings endpoint
  server.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    StaticJsonDocument<320> d;
    if (!isnan(tempC))        d["temperature"] = serialized(String(tempC, 1));
    if (!isnan(ecVoltage)) {
      d["ec"]         = serialized(String((ecVoltage - ecOffset) * ecSlope, 2));
      d["ec_voltage"] = serialized(String(ecVoltage, 3));
    }
    if (!isnan(waterPercent)) d["water_level"] = serialized(String(waterPercent, 1));
    d["ec_slope"]    = ecSlope;
    d["ec_offset"]   = ecOffset;
    d["water_v_min"] = waterVMin;
    d["water_v_max"] = waterVMax;
    String out; serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Sensor calibration endpoint: POST any combination of:
  //   ec_slope=<float>    EC slope  (mS/cm per volt, must be > 0)
  //   ec_offset=<float>   EC offset (volts at zero EC, -5 to +5)
  //   water_v_min=<float> Voltage when tank is empty (0–4.9 V)
  //   water_v_max=<float> Voltage when tank is full  (0.1–5 V, must be > water_v_min)
  server.on("/api/sensors/cal", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    bool changed = false;
    float newSlope  = ecSlope,  newOffset  = ecOffset;
    float newVMin   = waterVMin, newVMax   = waterVMax;

    if (r->hasParam("ec_slope", true)) {
      float v = r->getParam("ec_slope", true)->value().toFloat();
      if (v <= 0 || v > 1000) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"ec_slope out of range (0,1000]\"}"); return; }
      newSlope = v; changed = true;
    }
    if (r->hasParam("ec_offset", true)) {
      float v = r->getParam("ec_offset", true)->value().toFloat();
      if (v < -5.0f || v > 5.0f) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"ec_offset out of range [-5,5]\"}"); return; }
      newOffset = v; changed = true;
    }
    if (r->hasParam("water_v_min", true)) {
      float v = r->getParam("water_v_min", true)->value().toFloat();
      if (v < 0.0f || v >= 5.0f) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"water_v_min out of range [0,5)\"}"); return; }
      newVMin = v; changed = true;
    }
    if (r->hasParam("water_v_max", true)) {
      float v = r->getParam("water_v_max", true)->value().toFloat();
      if (v <= 0.0f || v > 5.0f) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"water_v_max out of range (0,5]\"}"); return; }
      newVMax = v; changed = true;
    }
    if (!changed) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"no calibration params provided\"}"); return; }
    if (newVMax - newVMin < 0.1f) { r->send(400, "application/json", "{\"ok\":false,\"error\":\"water_v_max must be at least 0.1V above water_v_min\"}"); return; }

    ecSlope = newSlope; ecOffset = newOffset;
    waterVMin = newVMin; waterVMax = newVMax;
    prefs.begin("sensor", false);
    prefs.putFloat("ec_sl",   ecSlope);
    prefs.putFloat("ec_off",  ecOffset);
    prefs.putFloat("wt_vmin", waterVMin);
    prefs.putFloat("wt_vmax", waterVMax);
    prefs.end();
    Serial.printf("[SENSOR] Cal updated: EC slope=%.3f offset=%.3f  Water vmin=%.3f vmax=%.3f\n",
                  ecSlope, ecOffset, waterVMin, waterVMax);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  server.begin();
  Serial.println("[STA] Web server started (Basic Auth ON).");
}

// -------------------- FS listing --------------------
static void listFiles(const char* dir) {
  File root = LittleFS.open(dir);
  if (!root || !root.isDirectory()) return;
  Serial.printf("[FS] %s:\n", dir);
  File f = root.openNextFile();
  while (f) { Serial.printf("  %s (%u)\n", f.name(), (unsigned)f.size()); f = root.openNextFile(); }
}

// -------------------- setup --------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== KC868-A6 boot ===");

  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED init — graceful if not connected
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 20);
    display.println("  KC868-A6");
    display.setCursor(0, 32);
    display.println("  Booting...");
    display.display();
    Serial.println("[OLED] Initialised.");
  } else {
    Serial.println("[OLED] Not found — skipping.");
  }

  relayMask = 0xFF;   // all relays OFF
  i2cWriteRelays();
  Serial.println("[I2C] Relay board initialised (all OFF).");

  // Set all input pins HIGH (quasi-bidirectional input mode)
  Wire.beginTransmission(I2C_INPUT_ADDR);
  Wire.write(0xFF);
  Wire.endTransmission();
  Serial.println("[I2C] Input board initialised.");

  // Read initial input states
  uint8_t inputByte = i2cReadInputs();
  for (int i = 0; i < NUM_INPUTS; i++) {
    int level = (inputByte >> i) & 0x01 ? HIGH : LOW;
    inputs[i].last_read      = level;
    inputs[i].stable         = level;
    inputs[i].last_change_ms = millis();
  }
  Serial.printf("[I2C] Input board read: 0x%02X\n", inputByte);

  WiFi.onEvent(onWiFiEvent);

  if (!LittleFS.begin(true)) {
    Serial.println("[FS] Mount failed.");
  } else {
    Serial.println("[FS] Mounted.");
    listFiles("/"); listFiles("/www");
  }

  deviceId = macToDeviceId();
  shortId  = macSuffix6();
  mdnsHost = "kc868a6-" + shortId;
  mdnsFqdn = mdnsHost + ".local";

  loadWifiCfg();
  loadMqttCfg();

  // Load sensor calibration from NVS
  prefs.begin("sensor", true);
  ecSlope   = prefs.getFloat("ec_sl",   1.0f);
  ecOffset  = prefs.getFloat("ec_off",  0.0f);
  waterVMin = prefs.getFloat("wt_vmin", 0.0f);
  waterVMax = prefs.getFloat("wt_vmax", 5.0f);
  prefs.end();
  Serial.printf("[SENSOR] EC slope=%.3f offset=%.3f  Water vmin=%.3f vmax=%.3f\n",
                ecSlope, ecOffset, waterVMin, waterVMax);

  // Init DS18B20
  tempSensor.setWaitForConversion(false);
  tempSensor.begin();
  Serial.printf("[DS18B20] Found %d sensor(s)\n", tempSensor.getDeviceCount());

  Serial.println("[ID] " + deviceId + "  mDNS: " + mdnsFqdn);

  if (connectSTA(20000)) {
    modeNow   = MODE_STA;
    wifiWasUp = true;
    startMDNS();
    setupRoutes_STA();
    mqttStart();
  } else {
    modeNow = MODE_AP;
    startAPPortal();
    setupRoutes_AP();
  }
  oledDirty = true;
}

// -------------------- loop --------------------
static uint32_t lastTelemetry  = 0;
static uint32_t lastSensorRead = 0;
static bool     tempRequested  = false;
static uint32_t tempRequestAt  = 0;

static const uint32_t TELEMETRY_INTERVAL_MS    = 30000;
static const uint32_t SENSOR_INTERVAL_MS       = 10000;
static const uint32_t DS18B20_CONV_MS          = 800;
static const uint32_t WIFI_FALLBACK_MS = 5UL * 60UL * 1000UL; // 5 min

static void fallbackToAP() {
  Serial.println("[WiFi] Lost for too long — switching to AP mode.");
  if (mqttHandle) {
    esp_mqtt_client_stop(mqttHandle);
    esp_mqtt_client_destroy(mqttHandle);
    mqttHandle    = nullptr;
    mqttConnected = false;
  }
  MDNS.end();
  server.end();
  modeNow = MODE_AP;
  startAPPortal();
  setupRoutes_AP();
  oledDirty = true;
}

void loop() {
  if (modeNow == MODE_AP) {
    dns.processNextRequest();
    delay(10);
    return;
  }

  // Runtime WiFi-drop → AP fallback
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiWasUp) {
      wifiWasUp  = true;
      wifiDropAt = 0;
      Serial.println("[WiFi] Reconnected.");
    }
  } else {
    if (wifiWasUp) {
      wifiWasUp  = false;
      wifiDropAt = millis();
      Serial.println("[WiFi] Connection lost — fallback timer started.");
    }
    if (wifiDropAt && (millis() - wifiDropAt) >= WIFI_FALLBACK_MS) {
      fallbackToAP();
      return;
    }
  }

  uint32_t now = millis();

  // OLED page cycling
  if (oledOk && (now - oledPageAt) >= OLED_PAGE_MS) {
    oledPageAt = now;
    oledPage   = (oledPage + 1) % 2;
    oledDirty  = true;
  }

  // Rate-limit OLED updates — transferring the full framebuffer over I2C
  // blocks the bus for ~50ms and delays input polling when updated every loop.
  static uint32_t lastOledUpdate = 0;
  if (oledDirty && (now - lastOledUpdate) >= 100) {
    updateOLED();
    lastOledUpdate = now;
  }

  // Poll I2C input board
  uint8_t inputByte = i2cReadInputs();
  for (int i = 0; i < NUM_INPUTS; i++) {
    int level = (inputByte >> i) & 0x01 ? HIGH : LOW;
    if (level != inputs[i].last_read) {
      inputs[i].last_read      = level;
      inputs[i].last_change_ms = now;
    }
    if ((now - inputs[i].last_change_ms) > INPUT_DEBOUNCE_MS &&
        inputs[i].stable != inputs[i].last_read) {
      inputs[i].stable = inputs[i].last_read;
      const bool closed = (inputs[i].stable == LOW);
      Serial.printf("[DIN %d] %s\n", i + 1, closed ? "CLOSED" : "OPEN");
      setRelay(i, closed);
      oledDirty = true;
      publishInputStateOne(i);
      publishTelemetry();
    }
  }

  // Sensor reading — non-blocking DS18B20 + ADC
  if (!tempRequested && (now - lastSensorRead) >= SENSOR_INTERVAL_MS) {
    tempSensor.requestTemperatures();
    tempRequested = true;
    tempRequestAt = now;
  }
  if (tempRequested && (now - tempRequestAt) >= DS18B20_CONV_MS) {
    float t = tempSensor.getTempCByIndex(0);
    tempC     = (t == DEVICE_DISCONNECTED_C) ? NAN : t;
    ecVoltage = analogRead(PIN_EC_ADC) * (5.0f / 4095.0f);
    {
      float wv  = analogRead(PIN_WATER_ADC) * (5.0f / 4095.0f);
      float span = waterVMax - waterVMin;
      if (span < 0.01f) {
        waterPercent = 0.0f;
      } else {
        waterPercent = (wv - waterVMin) / span * 100.0f;
        if (waterPercent < 0.0f)   waterPercent = 0.0f;
        if (waterPercent > 100.0f) waterPercent = 100.0f;
      }
    }
    tempRequested  = false;
    lastSensorRead = now;
    oledDirty = true;
    Serial.printf("[SENSOR] T=%.1f C  EC=%.3fV (%.2f mS/cm)  Water=%.1f%%\n",
                  isnan(tempC) ? 0 : tempC,
                  isnan(ecVoltage) ? 0 : ecVoltage,
                  isnan(ecVoltage) ? 0 : (ecVoltage - ecOffset) * ecSlope,
                  isnan(waterPercent) ? 0 : waterPercent);
    publishTelemetry();
  }

  // Periodic telemetry heartbeat
  if (mqttConnected && (now - lastTelemetry) >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    publishTelemetry();
  }

  delay(10);
}
