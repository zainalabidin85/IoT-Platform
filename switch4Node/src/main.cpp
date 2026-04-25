/**************************************************************
 * ESP32 4-Relay Controller (STA UI protected with HTTP Basic Auth)
 *
 * ✅ Basic Auth (always ON in STA mode)
 *    - Protects: "/", "/settings", "/api/*", and static files under /www
 *    - AP captive portal remains OPEN for provisioning
 *
 * Basic Auth credentials:
 *   USER: admin
 *   PASS: switch4node
 *
 * GPIO:
 *  - Relays GPIO: 32, 33, 25, 26 (ACTIVE HIGH by default)
 *  - Inputs GPIO: 16, 17, 18, 19  (INPUT_PULLUP, dry contact to GND)
 *
 * MQTT (PURE per-relay topics + per-input topics):
 *  Base topic (config field: cmdTopic) example:
 *    home/switch/switch4node-A1B2C3
 *
 *  Relays:
 *    Command: <base>/relay/1/set        payload: ON|OFF|1|0|TOGGLE
 *    State:   <base>/relay/1/state      payload: ON|OFF  (retained)
 *    ... relay 2..4
 *
 *  Inputs (binary sensor style):
 *    State:   <base>/input/1/state      payload: ON|OFF  (retained)
 *            ON  = CLOSED (input pulled LOW, contact to GND)
 *            OFF = OPEN   (input HIGH)
 *
 *  Availability (optional but useful for HA):
 *    <base>/status payload: online/offline (retained)
 *
 * Notes:
 *  - “stateTopic” in settings is unused for per-relay mode (kept for backward compatibility)
 *  - AP mode does NOT serve the full /www folder (prevents accessing STA pages from AP)
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
#include <ESPmDNS.h>
#include "esp_wifi.h"

// -------------------- GPIO --------------------
#define RELAY1_PIN 32
#define RELAY2_PIN 33
#define RELAY3_PIN 25
#define RELAY4_PIN 26

#define INPUT1_PIN 16
#define INPUT2_PIN 17
#define INPUT3_PIN 18
#define INPUT4_PIN 19

const int relayPins[4] = {RELAY1_PIN, RELAY2_PIN, RELAY3_PIN, RELAY4_PIN};
const int inputPins[4] = {INPUT1_PIN, INPUT2_PIN, INPUT3_PIN, INPUT4_PIN};

#define RELAY_ACTIVE_LOW 0   // 0 = ACTIVE HIGH, 1 = ACTIVE LOW

// -------------------- FS/DNS ------------------
static const char* FS_ROOT = "/www";
static const byte DNS_PORT = 53;

// -------------------- Debounce ----------------
static const uint32_t INPUT_DEBOUNCE_MS = 50;

// -------------------- Web/MQTT ----------------
AsyncWebServer server(80);
DNSServer dns;

static esp_mqtt_client_handle_t mqttHandle    = nullptr;
static volatile bool             mqttConnected = false;
Preferences prefs;

static const char* FW_VERSION       = "1.0.0";
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "switch4Node";
static String _otaTopic;

static void performOTA(const String& url) {
  Serial.printf("[OTA] Starting from: %s\n", url.c_str());
  WiFiClient httpClient;
  httpUpdate.rebootOnUpdate(true);
  auto ret = httpUpdate.update(httpClient, url);
  if (ret == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
}

// -------------------- BASIC AUTH (STA) --------
static const bool  BASIC_AUTH_ON = true;
static const char* BASIC_USER   = "admin";
static const char* BASIC_PASS   = "switch4node";

// -------------------- State -------------------
bool relayState[4] = {false, false, false, false};

// Debounced input states (INPUT_PULLUP)
struct DebouncedInput {
  int last_read;
  int stable;
  uint32_t last_change_ms;
} inputs[4];

// IDs
String deviceId;
String shortId;
String mdnsHost;
String mdnsFqdn;

// WiFi config
struct WifiCfg {
  String ssid;
  String pass;
} wifiCfg;

// MQTT config
struct MqttCfg {
  bool   enabled = false;
  String uri;          // e.g. "wss://mqtt-iot.unitani.com/mqtt" or "mqtt://10.0.0.1:1883"
  String user;
  String pass;
  String cmdTopic;     // BASE TOPIC
  String stateTopic;   // unused, kept for NVS compat
} mqttCfg;

// Derived topics
String baseTopic;         // = mqttCfg.cmdTopic
String tAvail;            // <base>/status
String tRelaySetWild;     // <base>/relay/+/set
String tRelaySetAll;      // <base>/relay/set  (optional "all relays" JSON)
String tRelayStatePrefix; // <base>/relay/
String tInputStatePrefix; // <base>/input/

// -------------------- Helpers -----------------
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
  tRelaySetAll      = baseTopic + "/relay/set";
  tRelayStatePrefix = baseTopic + "/relay/";
  tInputStatePrefix = baseTopic + "/input/";
}

static inline String relaySetTopic(int relayIdx0) {
  return tRelayStatePrefix + String(relayIdx0 + 1) + "/set";
}
static inline String relayStateTopic(int relayIdx0) {
  return tRelayStatePrefix + String(relayIdx0 + 1) + "/state";
}
static inline String inputStateTopic(int inputIdx0) {
  return tInputStatePrefix + String(inputIdx0 + 1) + "/state";
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
    case 4:  return "ASSOC_EXPIRE";
    case 5:  return "ASSOC_TOOMANY";
    case 6:  return "NOT_AUTHED";
    case 7:  return "NOT_ASSOCED";
    case 8:  return "ASSOC_LEAVE";
    case 15: return "4WAY_HANDSHAKE_TIMEOUT";
    case 16: return "GROUP_KEY_UPDATE_TIMEOUT";
    case 17: return "IE_IN_4WAY_DIFFERS";
    case 18: return "GROUP_CIPHER_INVALID";
    case 19: return "PAIRWISE_CIPHER_INVALID";
    case 20: return "AKMP_INVALID";
    case 21: return "UNSUPP_RSN_IE_VERSION";
    case 22: return "INVALID_RSN_IE_CAP";
    case 23: return "802_1X_AUTH_FAILED";
    case 24: return "CIPHER_SUITE_REJECTED";
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
      Serial.println("[WiFiEvent] STA_CONNECTED");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WiFiEvent] GOT_IP: %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WiFiEvent] STA_DISCONNECTED reason=%d (%s)\n",
                    (int)info.wifi_sta_disconnected.reason,
                    wifiDiscReasonStr((int)info.wifi_sta_disconnected.reason));
      break;
    default:
      break;
  }
}

// -------------------- Basic Auth helpers (STA only) --------------------
static inline bool authOK(AsyncWebServerRequest *r) {
  if (!BASIC_AUTH_ON) return true;
  return r->authenticate(BASIC_USER, BASIC_PASS);
}

static inline bool requireAuthOr401(AsyncWebServerRequest *r) {
  if (authOK(r)) return true;
  r->requestAuthentication();
  return false;
}

// -------------------- Relay / Input publish --------------------
static void mqttPublishRetained(const String& topic, const char* payload) {
  if (!mqttConnected) return;
  esp_mqtt_client_publish(mqttHandle, topic.c_str(), payload, 0, 1, 1);
}

static void publishAvailability(bool online) {
  if (!mqttConnected || !tAvail.length()) return;
  mqttPublishRetained(tAvail, online ? "online" : "offline");
}

static void publishRelayStateOne(int relayIdx0) {
  if (!mqttConnected || !baseTopic.length()) return;
  const String t = relayStateTopic(relayIdx0);
  mqttPublishRetained(t, relayState[relayIdx0] ? "ON" : "OFF");
}

static void publishAllRelayStates() {
  for (int i = 0; i < 4; i++) publishRelayStateOne(i);
}

static void publishInputStateOne(int inputIdx0) {
  if (!mqttConnected || !baseTopic.length()) return;
  const bool closed = (inputs[inputIdx0].stable == LOW);
  const String t = inputStateTopic(inputIdx0);
  mqttPublishRetained(t, closed ? "ON" : "OFF");
}

static void publishAllInputStates() {
  for (int i = 0; i < 4; i++) publishInputStateOne(i);
}

static void publishTelemetry() {
  if (!mqttConnected || !baseTopic.length()) return;
  StaticJsonDocument<256> doc;
  for (int i = 0; i < 4; i++) {
    char key[8];
    snprintf(key, sizeof(key), "relay%d", i + 1);
    doc[key] = relayState[i] ? 1 : 0;
  }
  for (int i = 0; i < 4; i++) {
    char key[8];
    snprintf(key, sizeof(key), "input%d", i + 1);
    doc[key] = (inputs[i].stable == LOW) ? 1 : 0;
  }
  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (baseTopic + "/telemetry").c_str(), buf, 0, 0, 0);
}

static void setRelay(int relayNum, bool on) {
  if (relayNum < 0 || relayNum >= 4) return;

  relayState[relayNum] = on;
  const int level = RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
  digitalWrite(relayPins[relayNum], level);

  Serial.printf("[RELAY %d] %s (GPIO level=%d)\n", relayNum + 1, on ? "ON" : "OFF", level);

  publishRelayStateOne(relayNum);
  publishTelemetry();
}

static void toggleRelay(int relayNum) {
  setRelay(relayNum, !relayState[relayNum]);
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
  // Migrate old host:port NVS to URI
  if (!mqttCfg.uri.length()) {
    String host = prefs.getString("host", "");
    uint16_t port = prefs.getUShort("port", 1883);
    if (host.length()) mqttCfg.uri = "mqtt://" + host + ":" + String(port);
  }
  prefs.end();
  applyTopics();
}

static void saveMqttCfg() {
  prefs.begin("mqtt", false);
  prefs.putBool("en",    mqttCfg.enabled);
  prefs.putString("uri", mqttCfg.uri);
  prefs.putString("user", mqttCfg.user);
  prefs.putString("pass", mqttCfg.pass);
  prefs.putString("cmd",  mqttCfg.cmdTopic);
  prefs.putString("st",   mqttCfg.stateTopic);
  prefs.end();
}

// -------------------- WiFi --------------------
static bool connectSTA(uint32_t timeoutMs = 20000) {
  if (!wifiCfg.ssid.length()) {
    Serial.println("[WiFi] No SSID saved.");
    return false;
  }

  Serial.println("[WiFi] connectSTA()");
  Serial.println("[WiFi] Saved SSID = [" + wifiCfg.ssid + "]");
  Serial.println("[WiFi] Saved PASS length = " + String(wifiCfg.pass.length()));

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(mdnsHost.c_str());
  WiFi.setAutoReconnect(true);

  WiFi.disconnect(true, true);
  delay(200);

  Serial.println("[WiFi] Connecting...");
  WiFi.begin(wifiCfg.ssid.c_str(), wifiCfg.pass.c_str());

  wl_status_t last = WL_IDLE_STATUS;
  uint32_t t0 = millis();

  while (millis() - t0 < timeoutMs) {
    wl_status_t st = WiFi.status();
    if (st != last) {
      last = st;
      Serial.printf("[WiFi] status=%d (%s)\n", (int)st, wlStatusStr(st));
    }
    if (st == WL_CONNECTED) {
      Serial.printf("[WiFi] Connected! IP=%s RSSI=%d\n",
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
      return true;
    }
    delay(250);
  }

  Serial.printf("[WiFi] Timeout. Final status=%d (%s)\n",
                (int)WiFi.status(), wlStatusStr(WiFi.status()));
  return false;
}

static void startAPPortal() {
  WiFi.disconnect(true, true);
  delay(200);

  const String apSsid = "Switch4Node-" + deviceId;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid.c_str(), nullptr);
  delay(200);

  const IPAddress ip = WiFi.softAPIP();
  dns.start(DNS_PORT, "*", ip);

  Serial.println("[AP] Mode SSID: " + apSsid);
  Serial.println("[AP] IP: " + ip.toString());
}

// -------------------- mDNS --------------------
static void startMDNS() {
  if (MDNS.begin(mdnsHost.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("[mDNS] http://" + mdnsFqdn + "/");
  } else {
    Serial.println("[mDNS] start failed");
  }
}

// -------------------- MQTT --------------------
static bool mqttReady() {
  if (!mqttCfg.enabled) return false;
  if (!mqttCfg.uri.length()) return false;
  if (!baseTopic.length()) return false;
  return true;
}

static bool parseOnOffToggle(const String& s, bool &outOn, bool &isToggle) {
  String v = s;
  v.trim();
  v.toUpperCase();

  isToggle = false;

  if (v == "TOGGLE") { isToggle = true; return true; }
  if (v == "ON" || v == "1" || v == "TRUE")  { outOn = true;  return true; }
  if (v == "OFF"|| v == "0" || v == "FALSE") { outOn = false; return true; }
  return false;
}

// handle <base>/relay/<n>/set
static bool handleRelaySetTopic(const String& topic, const String& payload) {
  // Expect: baseTopic + "/relay/" + n + "/set"
  const String prefix = baseTopic + "/relay/";
  if (!topic.startsWith(prefix)) return false;

  String rest = topic.substring(prefix.length()); // "<n>/set" maybe
  int slash = rest.indexOf('/');
  if (slash < 0) return false;

  String nStr = rest.substring(0, slash);
  String tail = rest.substring(slash + 1); // "set" or something
  if (tail != "set") return false;

  int n = nStr.toInt();
  if (n < 1 || n > 4) return false;

  bool on = false, isToggle = false;
  if (!parseOnOffToggle(payload, on, isToggle)) {
    Serial.printf("[MQTT] invalid payload for relay: %s\n", payload.c_str());
    return true; // topic matched, but payload invalid
  }

  if (isToggle) toggleRelay(n - 1);
  else setRelay(n - 1, on);

  return true;
}

// optional: <base>/relay/set  with JSON {"1":"ON","2":"OFF"...}
static bool handleRelaySetAllTopic(const String& topic, const String& payload) {
  if (topic != tRelaySetAll) return false;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("[MQTT] relay/set invalid JSON");
    return true;
  }
  for (int i = 1; i <= 4; i++) {
    char key[2]; sprintf(key, "%d", i);
    if (!doc.containsKey(key)) continue;
    String val = doc[key].as<String>();
    bool on = false, isToggle = false;
    if (!parseOnOffToggle(val, on, isToggle)) continue;
    if (isToggle) toggleRelay(i - 1);
    else setRelay(i - 1, on);
  }
  return true;
}

static void mqttOnMessage(const String& topic, const String& msg) {
  Serial.printf("[MQTT] RX topic=%s payload=%s\n", topic.c_str(), msg.c_str());
  if (handleRelaySetTopic(topic, msg)) return;
  if (handleRelaySetAllTopic(topic, msg)) return;
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
    return;
  }
  Serial.println("[MQTT] Unhandled topic");
}

static void mqttEventHandler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("[MQTT] Connected.");
      _otaTopic = baseTopic + "/ota/command";
      esp_mqtt_client_subscribe(mqttHandle, tRelaySetWild.c_str(), 1);
      esp_mqtt_client_subscribe(mqttHandle, tRelaySetAll.c_str(), 1);
      esp_mqtt_client_subscribe(mqttHandle, _otaTopic.c_str(), 1);
      publishAvailability(true);
      publishAllRelayStates();
      publishAllInputStates();
      publishTelemetry();
      break;
    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      Serial.println("[MQTT] Disconnected.");
      break;
    case MQTT_EVENT_DATA:
      if (ev->topic && ev->data) {
        mqttOnMessage(String(ev->topic, ev->topic_len),
                      String(ev->data,  ev->data_len));
      }
      break;
    default:
      break;
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
  cfg.uri                      = mqttCfg.uri.c_str();
  cfg.client_id                = clientId.c_str();
  cfg.username                 = mqttCfg.user.c_str();
  cfg.password                 = mqttCfg.pass.c_str();
  cfg.lwt_topic                = tAvail.c_str();
  cfg.lwt_msg                  = "offline";
  cfg.lwt_msg_len              = 7;
  cfg.lwt_qos                  = 1;
  cfg.lwt_retain               = 1;
  cfg.reconnect_timeout_ms     = 5000;
  cfg.cert_pem                 = ISRG_ROOT_X1;

  Serial.printf("[MQTT] Starting — uri=%s user=%s base=%s\n",
                mqttCfg.uri.c_str(), mqttCfg.user.c_str(), baseTopic.c_str());

  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

// -------------------- Platform provisioning --------------------
static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Switch4 Node - Provision</title>
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
<input id="topic" placeholder="e.g. mybuilding-switch4-1" required>
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

static void doProvision(const String& topic, const String& apiKey,
                        AsyncWebServerRequest* r) {
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
        prefs.putBool("done",     true);
        prefs.putString("apikey", apiKey);
        prefs.end();

        r->send(200, "application/json", "{\"ok\":true}");
        return;
      }
    }
    r->send(502, "application/json", "{\"ok\":false,\"error\":\"bad_response\"}");
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
  r->send(code == 409 ? 409 : 502, "application/json", out);
}

// -------------------- Web routes (AP mode) --------------------
static void setupRoutes_AP() {
  // Captive portal probe endpoints
  server.on("/connecttest.txt", HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/ncc.txt",         HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/generate_204",    HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/hotspot-detect.html", HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/fwlink",          HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/canonical.html",  HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/success.txt",     HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/library/test/success.html", HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/redirect",        HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/ncsi.txt",        HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });
  server.on("/chromehotstart.crx", HTTP_ANY, [](AsyncWebServerRequest *r){ r->redirect("/"); });

  // AP main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });

  // Minimal static assets for AP (IMPORTANT: do NOT expose full /www in AP mode)
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *r){
    if (LittleFS.exists("/www/style.css")) r->send(LittleFS, "/www/style.css", "text/css");
    else r->send(404, "text/plain", "missing");
  });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *r){
    if (LittleFS.exists("/www/app.js")) r->send(LittleFS, "/www/app.js", "application/javascript");
    else r->send(404, "text/plain", "missing");
  });

  // Status endpoint for AP mode
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    String json = "{\"ok\":true,\"mode\":\"ap\",\"mdns\":\"" + mdnsFqdn + "\"}";
    r->send(200, "application/json", json);
  });

  // WiFi scan endpoint
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *r){
    Serial.println("[AP] Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
      json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
      json += "\"encryption\":\"" + String(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SECURE") + "\"}";
    }
    json += "]}";
    r->send(200, "application/json", json);
    WiFi.scanDelete();
  });

  // WiFi save endpoint
  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *r){
    Serial.println("[AP] /api/wifi POST received");

    auto v = [&](const char* k)->String{
      if (r->hasParam(k, true)) return r->getParam(k, true)->value();
      return "";
    };

    const String ssid = v("ssid");
    const String pass = v("pass");

    if (!ssid.length()) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"ssid_required\"}");
      return;
    }

    wifiCfg.ssid = ssid;
    wifiCfg.pass = pass;
    saveWifiCfg();

    r->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    delay(500);
    Serial.println("[AP] Rebooting now...");
    ESP.restart();
  });

  server.onNotFound([](AsyncWebServerRequest *r){
    r->redirect("/");
  });

  server.begin();
  Serial.println("[AP] Web server started (open).");
}

// -------------------- Web routes (STA mode) --------------------
static void setupRoutes_STA() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send(LittleFS, "/www/index.html", "text/html");
  });

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send(LittleFS, "/www/settings.html", "text/html");
  });

  // Static under auth
  {
    auto &h = server.serveStatic("/", LittleFS, FS_ROOT);
    h.setFilter([](AsyncWebServerRequest *r){
      return authOK(r);
    });
  }

  // Platform provisioning page
  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    r->send_P(200, "text/html", PROVISION_HTML);
  });

  // Platform provisioning action — takes topic + api_key, fetches MQTT creds
  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;
    if (!r->hasParam("topic", true) || !r->hasParam("api_key", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"missing_params\"}");
      return;
    }
    String topic  = r->getParam("topic",   true)->value();
    String apiKey = r->getParam("api_key", true)->value();
    topic.trim(); apiKey.trim();
    if (!topic.length() || !apiKey.length()) {
      r->send(400, "application/json", "{\"ok\":false,\"error\":\"empty_params\"}");
      return;
    }
    doProvision(topic, apiKey, r);
  });

  // Status endpoint
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;

    StaticJsonDocument<768> d;
    d["ok"] = true;
    d["mode"] = "sta";
    d["ip"] = WiFi.localIP().toString();
    d["mdns"] = mdnsFqdn;
    d["rssi"] = WiFi.RSSI();

    JsonArray relaysArray = d.createNestedArray("relays");
    for (int i = 0; i < 4; i++) relaysArray.add(relayState[i]);

    // Report closed/open explicitly
    JsonArray inputsClosed = d.createNestedArray("inputs_closed");
    for (int i = 0; i < 4; i++) inputsClosed.add(inputs[i].stable == LOW);

    d["mqtt_enabled"] = mqttCfg.enabled;
    d["mqtt_connected"] = mqttConnected;
    d["mqtt_uri"]       = mqttCfg.uri;
    d["mqtt_base"] = baseTopic;
    d["mqtt_availability"] = tAvail;

    String out;
    serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Relay control endpoint (form)
  server.on("/api/relay", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;

    if (!r->hasParam("relay", true) || !r->hasParam("state", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_params\"}");
      return;
    }

    int relayNum = r->getParam("relay", true)->value().toInt() - 1;
    if (relayNum < 0 || relayNum >= 4) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"invalid_relay\"}");
      return;
    }

    const String s = r->getParam("state", true)->value();
    bool on = (s == "1" || s.equalsIgnoreCase("on") || s.equalsIgnoreCase("true"));
    setRelay(relayNum, on);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // Batch relay control (JSON form field "states": {"1":"ON","2":"OFF"...}
  server.on("/api/relays", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;

    if (!r->hasParam("states", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_states\"}");
      return;
    }

    String statesJson = r->getParam("states", true)->value();
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, statesJson);

    if (error) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"invalid_json\"}");
      return;
    }

    for (int i = 0; i < 4; i++) {
      char key[2]; sprintf(key, "%d", i + 1);
      if (!doc.containsKey(key)) continue;
      String val = doc[key].as<String>();
      bool on = false, isToggle = false;
      if (!parseOnOffToggle(val, on, isToggle)) continue;
      if (isToggle) toggleRelay(i);
      else setRelay(i, on);
    }

    r->send(200, "application/json", "{\"ok\":true}");
  });

  // MQTT GET
  server.begin();
  Serial.println("[STA] Web server started (Basic Auth ON).");
}

// -------------------- FS listing (debug) --------------------
static void listFiles(const char* dirname) {
  File root = LittleFS.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.printf("[FS] Not a dir: %s\n", dirname);
    return;
  }
  Serial.printf("[FS] Listing: %s\n", dirname);
  File file = root.openNextFile();
  while (file) {
    Serial.printf("  %s  (%u)\n", file.name(), (unsigned)file.size());
    file = root.openNextFile();
  }
}

// -------------------- setup/loop --------------------
enum Mode { MODE_AP, MODE_STA };
Mode modeNow = MODE_AP;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("=== Switch4Node boot ===");

  WiFi.onEvent(onWiFiEvent);

  // Initialize relay pins (avoid calling setRelay() before MQTT is connected)
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    relayState[i] = false;
    const int level = RELAY_ACTIVE_LOW ? HIGH : LOW;
    digitalWrite(relayPins[i], level);
  }

  // Initialize input pins
  for (int i = 0; i < 4; i++) {
    pinMode(inputPins[i], INPUT_PULLUP);
    inputs[i].last_read = digitalRead(inputPins[i]);
    inputs[i].stable = inputs[i].last_read;
    inputs[i].last_change_ms = millis();
  }

  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed (formatted if needed).");
  } else {
    Serial.println("[FS] LittleFS mounted.");
    listFiles("/");
    listFiles("/www");
  }

  deviceId = macToDeviceId();
  shortId  = macSuffix6();
  mdnsHost = "switch4node-" + shortId;
  mdnsFqdn = mdnsHost + ".local";

  loadWifiCfg();
  loadMqttCfg();

  Serial.println("[ID] Device ID: " + deviceId);
  Serial.println("[ID] mDNS host:  " + mdnsHost);
  Serial.println(String("[AUTH] ") + (BASIC_AUTH_ON ? "ENABLED" : "disabled") + " user=" + BASIC_USER);

  if (connectSTA(20000)) {
    modeNow = MODE_STA;
    Serial.println("[WiFi] STA connected, IP: " + WiFi.localIP().toString());
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

  // Check all 4 inputs with debouncing
  uint32_t now = millis();

  for (int i = 0; i < 4; i++) {
    int level = digitalRead(inputPins[i]);

    if (level != inputs[i].last_read) {
      inputs[i].last_read = level;
      inputs[i].last_change_ms = now;
    }

    if ((now - inputs[i].last_change_ms) > INPUT_DEBOUNCE_MS && inputs[i].stable != inputs[i].last_read) {
      inputs[i].stable = inputs[i].last_read;

      const bool closed = (inputs[i].stable == LOW); // LOW = contact closed
      Serial.printf("[DIN %d] stable -> %s\n", i + 1, closed ? "CLOSED(LOW)" : "OPEN(HIGH)");

      // Publish input state (per-input topic, retained)
      publishInputStateOne(i);

      // Toggle corresponding relay on press/close (LOW)
      if (closed) {
        toggleRelay(i);
      }
    }
  }

  delay(10);
}