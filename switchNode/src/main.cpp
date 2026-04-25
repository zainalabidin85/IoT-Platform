/**************************************************************
 * ESP32 Single Relay Controller (STA UI protected with HTTP Basic Auth)
 *
 * ✅ Basic Auth (always ON in STA mode)
 *    - Protects: "/", "/settings", "/api/*", and static files under /www
 *    - AP captive portal remains OPEN for provisioning
 *
 * Basic Auth credentials:
 *   USER: admin
 *   PASS: switchnode
 *
 * Pins:
 *  - Relay GPIO: 16 (ACTIVE HIGH by default)
 *  - Input GPIO: 25 (INPUT_PULLUP, dry contact to GND)
 *
 * DEBUG:
 *  - Verbose WiFi connect status prints + event-based disconnect reasons
 *  - Prints stored SSID + password length at boot
 *  - AP captive portal probe endpoints handled to reduce VFS "open()" spam
 **************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <dirent.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "mqtt_client.h"
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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
#include <HTTPUpdate.h>
#include <HTTPClient.h>

#include <ESPmDNS.h>
#include "esp_wifi.h"

// -------------------- GPIO --------------------
#define RELAY_PIN 16
#define INPUT_PIN 25
#define RELAY_ACTIVE_LOW 0   // 0 = ACTIVE HIGH, 1 = ACTIVE LOW

// -------------------- FS/DNS ------------------
static const char* FS_ROOT = "/www";
static const byte DNS_PORT = 53;

// -------------------- Debounce ----------------
static const uint32_t INPUT_DEBOUNCE_MS = 50;

// -------------------- Web/MQTT ----------------
AsyncWebServer server(80);
DNSServer dns;

static esp_mqtt_client_handle_t mqttHandle  = nullptr;
static volatile bool             mqttConnected = false;
Preferences prefs;

static const char* FW_VERSION       = "1.0.0";
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "switchNode";
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
static const char* BASIC_PASS   = "switchnode";

// -------------------- State -------------------
bool relayState = false;

// Debounced input state (INPUT_PULLUP)
static int in_last_read = HIGH;
static int in_stable = HIGH;
static uint32_t in_last_change_ms = 0;

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
  String cmdTopic;
  String stateTopic;
} mqttCfg;

String topicCmd, topicState, topicDin, topicStatus;

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
  topicCmd    = mqttCfg.cmdTopic + "/set";   // platform publishes to <base>/set
  topicState  = mqttCfg.cmdTopic + "/state";
  topicDin    = mqttCfg.cmdTopic + "/din";
  topicStatus = mqttCfg.cmdTopic + "/status";
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
  // Common reasons (not exhaustive)
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

// -------------------- Forward declarations --------------------
static void publishTelemetry();

// -------------------- Relay --------------------
static void setRelay(bool on) {
  relayState = on;
  const int level = RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW);
  digitalWrite(RELAY_PIN, level);

  Serial.printf("[RELAY] setRelay(%s) -> GPIO=%d\n", on ? "ON" : "OFF", level);

  if (mqttConnected && topicState.length()) {
    esp_mqtt_client_publish(mqttHandle, topicState.c_str(), relayState ? "ON" : "OFF", 0, 1, 1);
    Serial.printf("[MQTT] publish state %s => %s\n", topicState.c_str(), relayState ? "ON" : "OFF");
    publishTelemetry();
  }
}

static void publishTelemetry() {
  if (!mqttConnected || !mqttCfg.cmdTopic.length()) return;
  StaticJsonDocument<128> doc;
  doc["relay"] = relayState ? 1 : 0;
  doc["input"] = (in_stable == LOW) ? 1 : 0;
  char buf[128];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (mqttCfg.cmdTopic + "/telemetry").c_str(), buf, 0, 0, 0);
}

// Input publishing
static void publishInputOpenBool(bool open) {
  if (!mqttConnected || !topicDin.length()) return;
  const char* payload = open ? "OFF" : "ON";
  esp_mqtt_client_publish(mqttHandle, topicDin.c_str(), payload, 0, 1, 1);
  Serial.printf("[MQTT] publish din %s => %s (open=%d)\n", topicDin.c_str(), payload, open ? 1 : 0);
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

  // Reset previous state
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
  Serial.printf("[WiFi] RSSI (if any)=%d\n", WiFi.RSSI());
  return false;
}

static void startAPPortal() {
  // Ensure STA is stopped
  WiFi.disconnect(true, true);
  delay(200);

  const String apSsid = "SwitchNode-" + deviceId;
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

// -------------------- MQTT (esp_mqtt — WebSocket + plain TCP) --------------------
static void mqttOnMessage(const String& topic, const String& msg) {
  Serial.printf("[MQTT] RX topic=%s payload=%s\n", topic.c_str(), msg.c_str());
  if (topic == topicCmd) {
    if (msg.equalsIgnoreCase("ON")  || msg == "1" || msg.equalsIgnoreCase("true"))  setRelay(true);
    if (msg.equalsIgnoreCase("OFF") || msg == "0" || msg.equalsIgnoreCase("false")) setRelay(false);
  }
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

static void mqttEventHandler(void* arg, esp_event_base_t base,
                              int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t ev = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("[MQTT] Connected.");
      _otaTopic = mqttCfg.cmdTopic + "/ota/command";
      esp_mqtt_client_subscribe(mqttHandle, topicCmd.c_str(), 1);
      esp_mqtt_client_subscribe(mqttHandle, _otaTopic.c_str(), 1);
      esp_mqtt_client_publish(mqttHandle, topicStatus.c_str(), "online", 0, 1, 1);
      esp_mqtt_client_publish(mqttHandle, topicState.c_str(), relayState ? "ON" : "OFF", 0, 1, 1);
      publishTelemetry();
      publishInputOpenBool(in_stable == HIGH);
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
  if (!mqttCfg.enabled || !mqttCfg.uri.length() || !mqttCfg.cmdTopic.length()) return;

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
  cfg.lwt_topic                = topicStatus.c_str();
  cfg.lwt_msg                  = "offline";
  cfg.lwt_msg_len              = 7;
  cfg.lwt_qos                  = 1;
  cfg.lwt_retain               = 1;
  cfg.reconnect_timeout_ms     = 5000;
  cfg.cert_pem                 = ISRG_ROOT_X1;

  Serial.printf("[MQTT] Starting — uri=%s user=%s\n",
                mqttCfg.uri.c_str(), mqttCfg.user.c_str());

  mqttHandle = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqttHandle, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqttEventHandler, nullptr);
  esp_mqtt_client_start(mqttHandle);
}

// -------------------- Platform provisioning --------------------
static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Switch Node - Provision</title>
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
<input id="topic" placeholder="e.g. myoffice-switch1" required>
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
        mqttCfg.uri        = cfg["uri"]    | "";
        mqttCfg.user       = cfg["user"]   | "";
        mqttCfg.pass       = cfg["pass"]   | "";
        mqttCfg.cmdTopic   = cfg["topic"]  | topic.c_str();
        mqttCfg.stateTopic = "";
        saveMqttCfg();
        applyTopics();
        mqttStart(); // restart with new credentials

        prefs.begin("prov", false);
        prefs.putBool("done",    true);
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

// -------------------- Web routes --------------------
static void setupRoutes_AP() {
  // Captive portal probe endpoints (avoid LittleFS "open()" errors)
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
  
  // Handle any .txt or .crx files
  server.onNotFound([](AsyncWebServerRequest *r){
    r->redirect("/");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/ap.html", "text/html");
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

  // Add this after the /api/scan endpoint, before /api/wifi
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    // In AP mode, just return the device ID and mDNS info
    String json = "{\"ok\":true,\"mdns\":\"" + mdnsFqdn + "\"}";
    r->send(200, "application/json", json);
  });

  server.on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *r){
    Serial.println("[AP] /api/wifi POST received");
    
    // Log all parameters received
    int params = r->params();
    Serial.printf("[AP] Number of params: %d\n", params);
    for (int i = 0; i < params; i++) {
        AsyncWebParameter* p = r->getParam(i);
        if (p->isPost()) {
            Serial.printf("[AP] POST param: %s = %s\n", 
                p->name().c_str(), 
                p->value().c_str());
        }
    }
    
    auto v = [&](const char* k)->String{
        if (r->hasParam(k, true)) {
            String val = r->getParam(k, true)->value();
            Serial.printf("[AP] Found param %s = %s\n", k, val.c_str());
            return val;
        }
        Serial.printf("[AP] Param %s not found!\n", k);
        return "";
    };

    const String ssid = v("ssid");
    const String pass = v("pass");

    Serial.println("[AP] /api/wifi POST ssid=[" + ssid + "] passLen=" + String(pass.length()));

    if (!ssid.length()) {
        Serial.println("[AP] ERROR: ssid required but not provided");
        r->send(400, "application/json", "{\"ok\":false,\"err\":\"ssid_required\"}");
        return;
    }

    wifiCfg.ssid = ssid;
    wifiCfg.pass = pass;
    saveWifiCfg();
    
    Serial.println("[AP] WiFi credentials saved, sending response and rebooting...");
    r->send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    
    // Flush the response before rebooting
    delay(500);
    Serial.println("[AP] Rebooting now...");
    ESP.restart();
  });

  server.onNotFound([](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(LittleFS, "/www/ap.html", "text/html");
  });

  server.serveStatic("/", LittleFS, FS_ROOT); // AP is open
  server.begin();

  Serial.println("[AP] Web server started (open).");
}

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

  // Status
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;

    StaticJsonDocument<640> d;
    d["ok"] = true;
    d["ip"] = WiFi.localIP().toString();
    d["mdns"] = mdnsFqdn;
    d["rssi"] = WiFi.RSSI();
    d["relay"] = relayState;
    d["input_pressed"] = (in_stable == LOW);
    d["mqtt_enabled"]   = mqttCfg.enabled;
    d["mqtt_connected"] = mqttConnected;
    d["mqtt_uri"]       = mqttCfg.uri;
    d["cmd_topic"]      = mqttCfg.cmdTopic;
    d["state_topic"] = topicState;
    d["din_topic"] = topicDin;

    String out;
    serializeJson(d, out);
    r->send(200, "application/json", out);
  });

  // Relay set
  server.on("/api/relay", HTTP_POST, [](AsyncWebServerRequest *r){
    if (!requireAuthOr401(r)) return;

    if (!r->hasParam("state", true)) {
      r->send(400, "application/json", "{\"ok\":false,\"err\":\"missing_state\"}");
      return;
    }
    const String s = r->getParam("state", true)->value();
    const bool on = (s == "1" || s.equalsIgnoreCase("on") || s.equalsIgnoreCase("true"));
    setRelay(on);
    r->send(200, "application/json", "{\"ok\":true}");
  });

  // MQTT GET (masked)
  server.begin();
  Serial.println("[STA] Web server started (Basic Auth ON).");
}

void listFiles(const char* dirname, uint8_t levels) {
  Serial.printf("Listing directory: %s\r\n", dirname);
  
  // Ensure the path starts with /
  String path = dirname;
  if (!path.startsWith("/")) {
    path = "/" + path;
  }
  
  File root = LittleFS.open(path);
  if (!root) {
    Serial.printf("- failed to open directory: %s\r\n", path.c_str());
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }
  
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if (levels) {
        listFiles(file.name(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
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
  Serial.println("=== SwitchNode boot ===");

  WiFi.onEvent(onWiFiEvent);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(INPUT_PIN, INPUT_PULLUP);

  setRelay(false);

  // Safer: do NOT format on fail in production.
  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed (formatted if needed).");
  } else {
    Serial.println("[FS] LittleFS mounted.");
    listFiles("/", 2);
    listFiles("/www", 1);  // Explicitly list www directory
  // Add debug file system check
  }

  deviceId = macToDeviceId();
  shortId  = macSuffix6();
  mdnsHost = "switchnode-" + shortId;
  mdnsFqdn = mdnsHost + ".local";

  loadWifiCfg();
  loadMqttCfg();

  in_last_read = digitalRead(INPUT_PIN);
  in_stable = in_last_read;
  in_last_change_ms = millis();

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

  // Dry contact debounce (INPUT_PULLUP)
  int level = digitalRead(INPUT_PIN);
  uint32_t now = millis();

  if (level != in_last_read) {
    in_last_read = level;
    in_last_change_ms = now;
  }

  if ((now - in_last_change_ms) > INPUT_DEBOUNCE_MS && in_stable != in_last_read) {
    in_stable = in_last_read;

    const bool isOpen = (in_stable == HIGH);
    Serial.printf("[DIN] stable change -> %s\n", isOpen ? "OPEN(HIGH)" : "CLOSED(LOW)");

    publishInputOpenBool(isOpen);

    // IMPORTANT: toggle only on press/close (LOW) to avoid double-toggle on release
    if (!isOpen) {
      setRelay(!relayState);
    }
  }

  delay(10);
}
