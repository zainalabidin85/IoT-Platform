/**************************************************************
 * rs485Node — Waveshare ESP32-S3-RS485-CAN
 *  - CWT-TH03S-H temperature/humidity probe (Modbus RTU, slave ID 1)
 *  - CWT CO2+Temp+Humidity integrated sensor (Modbus RTU, slave ID 1)
 *
 * Only one sensor is ever physically wired to the bus at a time —
 * both ship with the same factory default slave ID (1), so no
 * reassignment is needed. Which sensor is present is a runtime
 * choice, selected via the web dashboard (/api/sensor) and stored
 * in NVS; the firmware polls only that sensor's register map.
 *
 * Pins (Waveshare ESP32-S3-RS485-CAN onboard RS485 transceiver):
 *  - RS485 TX:    GPIO17
 *  - RS485 RX:    GPIO18
 *  - RS485 DE/RE: GPIO21
 *  - BOOT BTN:    GPIO0  (hold 5s at runtime -> factory reset WiFi/MQTT)
 *
 * Basic Auth (STA mode dashboard):
 *   USER: admin  PASS: rs485node
 **************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <LittleFS.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include <ArduinoJson.h>
#include "mqtt_client.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HTTPUpdate.h>
#include <ESPmDNS.h>
#include <ModbusMaster.h>

/**************************************************************
 * VERSION
 **************************************************************/
static const char* FW_VERSION       = "1.0.0";
static const uint8_t API_VERSION    = 1;
static const char* PLATFORM_API_URL = "https://api-iot.unitani.com";
static const char* NODE_TYPE        = "rs485node";

/**************************************************************
 * PROVISION PAGE (embedded fallback, served in STA mode)
 **************************************************************/
static const char PROVISION_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>RS485 Node - Platform Provision</title>
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
<input id="topic" placeholder="e.g. myfarm-rs485-1" required>
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
 * PINS
 **************************************************************/
static const int RS485_TX_PIN    = 17;
static const int RS485_RX_PIN    = 18;
static const int RS485_DE_RE_PIN = 21;
static const int BOOT_BTN_PIN    = 0;   // hold 5s at runtime -> factory reset

static const uint32_t FACTORY_RESET_HOLD_MS = 5000;

/**************************************************************
 * BASIC AUTH (STA dashboard)
 **************************************************************/
static const bool  BASIC_AUTH_ON = true;
static const char* BASIC_USER    = "admin";
static const char* BASIC_PASS    = "rs485node";

/**************************************************************
 * MODBUS
 **************************************************************/
static const uint8_t SENSOR_SLAVE_ID = 1;  // factory default for all supported sensors

// To add a new sensor type: add an enum value, a string mapping below, a
// baud entry in sensorBaud(), a read branch in sensorTick(), a JSON block
// in mqttPublishNow()/the /api/data route, and a matching <option> + card
// grid + SENSOR_CARD_IDS entry in the dashboard (data/www/index.html, app.js).
enum SensorType : uint8_t { SENSOR_TH = 0, SENSOR_CO2 = 1, SENSOR_EC = 2, SENSOR_PH = 3 };
static SensorType sensorType = SENSOR_TH;

static const char* sensorTypeStr(SensorType t){
  switch (t) {
    case SENSOR_CO2: return "co2";
    case SENSOR_EC:  return "ec";
    case SENSOR_PH:  return "ph";
    default:         return "th";
  }
}
static SensorType sensorTypeFromStr(const String& s){
  if (s == "co2") return SENSOR_CO2;
  if (s == "ec")  return SENSOR_EC;
  if (s == "ph")  return SENSOR_PH;
  return SENSOR_TH;
}

// CWT-TH03S-H and the CO2 sensor default to 4800 baud; the CWT-BL EC/TDS
// and pH transmitters default to 9600 baud — the UART must be
// reconfigured when switching between the two groups.
static uint32_t sensorBaud(SensorType t){
  return (t == SENSOR_EC || t == SENSOR_PH) ? 9600 : 4800;
}

/**************************************************************
 * TIMING
 **************************************************************/
static const uint32_t TICK_SENSOR_MS = 2000;
static const uint32_t TICK_MQTT_MS   = 200;

/**************************************************************
 * OBJECTS
 **************************************************************/
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

static esp_mqtt_client_handle_t mqttHandle = nullptr;
static volatile bool mqttConnected = false;
static String _otaTopic;
static String g_hostname;
static String pendingOtaUrl;   // set from MQTT callback, consumed in loop()
static bool g_mdnsStarted = false;

static void startMDNSOnce(){
  if (g_mdnsStarted) return;
  if (MDNS.begin(g_hostname.c_str())) MDNS.addService("http", "tcp", 80);
  g_mdnsStarted = true;
}

static void performOTA(const String& url) {
  Serial.printf("[OTA] Starting from: %s\n", url.c_str());
  WiFiClientSecure httpClient;
  httpClient.setInsecure();
  httpUpdate.rebootOnUpdate(true);
  auto ret = httpUpdate.update(httpClient, url);
  if (ret == HTTP_UPDATE_FAILED)
    Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
}

ModbusMaster modbusNode;

static void preTransmission()  { digitalWrite(RS485_DE_RE_PIN, HIGH); }
static void postTransmission() { digitalWrite(RS485_DE_RE_PIN, LOW);  }

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
  String base_topic = "rs485node";
  bool retain = true;
  uint16_t pub_period_ms = 2000;
};

struct MqttStatus {
  bool connected = false;
  uint32_t lastPublishMs = 0;
  String err = "";
};

struct Sensors {
  float humidity_percent = NAN;      // from CWT-TH03S-H (canonical)
  float temp_c = NAN;                // from CWT-TH03S-H (canonical)
  bool  th_valid = false;

  float co2_ppm = NAN;               // from CO2 integrated sensor
  float co2_temp_c = NAN;
  float co2_humidity_percent = NAN;
  bool  co2_valid = false;

  float ec_tds_value = NAN;          // from CWT-BL EC/TDS transmitter (unit depends on configured range)
  bool  ec_valid = false;

  float ph_value = NAN;              // from CWT-BL pH transmitter
  float ph_temp_c = NAN;
  bool  ph_valid = false;
};

static WifiStatus wifiSt;
static MqttConfig mqttCfg;
static MqttStatus mqttSt;
static Sensors sens;
static bool apMode = false;

/**************************************************************
 * HELPERS
 **************************************************************/
static String ipToString(const IPAddress& ip){
  return String(ip[0])+"."+String(ip[1])+"."+String(ip[2])+"."+String(ip[3]);
}

static String chipSuffix(){
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

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

/**************************************************************
 * AUTH
 **************************************************************/
static inline bool authOK(AsyncWebServerRequest* r) {
  if (!BASIC_AUTH_ON) return true;
  return r->authenticate(BASIC_USER, BASIC_PASS);
}
static inline bool requireAuthOr401(AsyncWebServerRequest* r) {
  if (authOK(r)) return true;
  r->requestAuthentication();
  return false;
}

/**************************************************************
 * WIFI / CAPTIVE PORTAL
 **************************************************************/
static void startAP(){
  apMode = true;
  WiFi.mode(WIFI_AP);
  String apSsid = String("RS485Node-") + chipSuffix();
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

      startMDNSOnce();
    } else {
      wifiSt.mode = WifiStatus::WIFI_STA;
      wifiSt.connected = false;
      wifiSt.ip = "";
      wifiSt.ssid = "";

      // Nudge reconnect periodically as a safety net. Never falls back to
      // AP mode once provisioned — a temporarily unreachable router is not
      // treated as "unprovisioned".
      static uint32_t lastReconnect = 0;
      uint32_t now = millis();
      if (now - lastReconnect > 30000) {
        lastReconnect = now;
        WiFi.reconnect();
      }
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
  mqttCfg.base_topic    = prefs.getString("topic", "rs485node");
  mqttCfg.retain        = prefs.getBool("ret", true);
  mqttCfg.pub_period_ms = (uint16_t)prefs.getUShort("per", 2000);
  prefs.end();

  prefs.begin("prov", true);
  bool provisioned = prefs.getBool("done", false);
  prefs.end();
  if (provisioned && mqttCfg.uri.length() > 0) {
    mqttCfg.enabled = true;
  }
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
 * PREFERENCES: SENSOR SELECTION
 **************************************************************/
static void loadSensorType(){
  prefs.begin("sensor", true);
  sensorType = (SensorType)prefs.getUChar("type", (uint8_t)SENSOR_TH);
  prefs.end();
}

static void saveSensorType(SensorType t){
  prefs.begin("sensor", false);
  prefs.putUChar("type", (uint8_t)t);
  prefs.end();

  bool baudChanged = sensorBaud(t) != sensorBaud(sensorType);
  sensorType = t;
  if (baudChanged) {
    Serial2.begin(sensorBaud(sensorType), SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
  }
}

/**************************************************************
 * MODBUS / SENSORS
 **************************************************************/
static void clearInactiveSensorReadings(){
  if (sensorType != SENSOR_TH) {
    sens.th_valid = false;
    sens.temp_c = sens.humidity_percent = NAN;
  }
  if (sensorType != SENSOR_CO2) {
    sens.co2_valid = false;
    sens.co2_ppm = sens.co2_temp_c = sens.co2_humidity_percent = NAN;
  }
  if (sensorType != SENSOR_EC) {
    sens.ec_valid = false;
    sens.ec_tds_value = NAN;
  }
  if (sensorType != SENSOR_PH) {
    sens.ph_valid = false;
    sens.ph_value = sens.ph_temp_c = NAN;
  }
}

static void sensorTick(){
  clearInactiveSensorReadings();

  switch (sensorType) {
    case SENSOR_TH: {
      // CWT-TH03S-H: 0x0000 humidity (0.1%RH), 0x0001 temperature (0.1C, int16)
      uint8_t r = modbusNode.readHoldingRegisters(0x0000, 2);
      if (r == modbusNode.ku8MBSuccess) {
        uint16_t rawHum  = modbusNode.getResponseBuffer(0);
        int16_t  rawTemp = (int16_t)modbusNode.getResponseBuffer(1);
        sens.humidity_percent = rawHum / 10.0f;
        sens.temp_c = rawTemp / 10.0f;
        sens.th_valid = true;
      } else {
        sens.th_valid = false;
        Serial.printf("[Modbus] TH sensor error: 0x%02X\n", r);
      }
      break;
    }
    case SENSOR_CO2: {
      // CO2 integrated sensor: 0x0000 humidity, 0x0001 temperature, 0x0002 CO2
      uint8_t r = modbusNode.readHoldingRegisters(0x0000, 3);
      if (r == modbusNode.ku8MBSuccess) {
        uint16_t rawHum  = modbusNode.getResponseBuffer(0);
        int16_t  rawTemp = (int16_t)modbusNode.getResponseBuffer(1);
        uint16_t rawCo2  = modbusNode.getResponseBuffer(2);
        sens.co2_humidity_percent = rawHum / 10.0f;
        sens.co2_temp_c = rawTemp / 10.0f;
        sens.co2_ppm = rawCo2;
        sens.co2_valid = true;
      } else {
        sens.co2_valid = false;
        Serial.printf("[Modbus] CO2 sensor error: 0x%02X\n", r);
      }
      break;
    }
    case SENSOR_EC: {
      // CWT-BL EC/TDS: 0x0000 calibration-solution value (unused here), 0x0001 EC/TDS value (0.1 scale)
      uint8_t r = modbusNode.readHoldingRegisters(0x0000, 2);
      if (r == modbusNode.ku8MBSuccess) {
        uint16_t rawVal = modbusNode.getResponseBuffer(1);
        sens.ec_tds_value = rawVal / 10.0f;
        sens.ec_valid = true;
      } else {
        sens.ec_valid = false;
        Serial.printf("[Modbus] EC/TDS sensor error: 0x%02X\n", r);
      }
      break;
    }
    case SENSOR_PH: {
      // CWT-BL pH: 0x0000 temperature (0.1C, int16), 0x0001 pH (0.1 scale)
      uint8_t r = modbusNode.readHoldingRegisters(0x0000, 2);
      if (r == modbusNode.ku8MBSuccess) {
        int16_t  rawTemp = (int16_t)modbusNode.getResponseBuffer(0);
        uint16_t rawPh   = modbusNode.getResponseBuffer(1);
        sens.ph_temp_c = rawTemp / 10.0f;
        sens.ph_value = rawPh / 10.0f;
        sens.ph_valid = true;
      } else {
        sens.ph_valid = false;
        Serial.printf("[Modbus] pH sensor error: 0x%02X\n", r);
      }
      break;
    }
  }
}

/**************************************************************
 * MQTT (esp_mqtt_client)
 **************************************************************/
static String mqttBase(){
  String base = mqttCfg.base_topic;
  if (!base.length()) base = "rs485node";
  while (base.endsWith("/")) base.remove(base.length() - 1);
  return base;
}

static void mqttPublishNow(){
  if (!mqttHandle || !mqttConnected) return;

  String base = mqttBase();
  int qos = 0;
  bool retain = mqttCfg.retain;

  StaticJsonDocument<384> doc;
  doc["sensor_type"] = sensorTypeStr(sensorType);
  switch (sensorType) {
    case SENSOR_TH:
      doc["humidity_percent"] = sens.humidity_percent;
      doc["temp_c"] = sens.temp_c;
      break;
    case SENSOR_CO2:
      doc["co2_ppm"] = sens.co2_ppm;
      doc["co2_temp_c"] = sens.co2_temp_c;
      doc["co2_humidity_percent"] = sens.co2_humidity_percent;
      break;
    case SENSOR_EC:
      doc["ec_tds_value"] = sens.ec_tds_value;
      break;
    case SENSOR_PH:
      doc["ph_value"] = sens.ph_value;
      doc["ph_temp_c"] = sens.ph_temp_c;
      break;
  }

  char buf[384];
  serializeJson(doc, buf, sizeof(buf));
  esp_mqtt_client_publish(mqttHandle, (base + "/telemetry").c_str(), buf, 0, qos, retain);

  char tmp[24];
  switch (sensorType) {
    case SENSOR_TH:
      if (!isnan(sens.temp_c)) {
        dtostrf(sens.temp_c, 0, 1, tmp);
        esp_mqtt_client_publish(mqttHandle, (base + "/temp_c").c_str(), tmp, 0, qos, retain);
      }
      if (!isnan(sens.humidity_percent)) {
        dtostrf(sens.humidity_percent, 0, 1, tmp);
        esp_mqtt_client_publish(mqttHandle, (base + "/humidity").c_str(), tmp, 0, qos, retain);
      }
      break;
    case SENSOR_CO2:
      if (!isnan(sens.co2_ppm)) {
        dtostrf(sens.co2_ppm, 0, 0, tmp);
        esp_mqtt_client_publish(mqttHandle, (base + "/co2_ppm").c_str(), tmp, 0, qos, retain);
      }
      break;
    case SENSOR_EC:
      if (!isnan(sens.ec_tds_value)) {
        dtostrf(sens.ec_tds_value, 0, 1, tmp);
        esp_mqtt_client_publish(mqttHandle, (base + "/ec_tds").c_str(), tmp, 0, qos, retain);
      }
      break;
    case SENSOR_PH:
      if (!isnan(sens.ph_value)) {
        dtostrf(sens.ph_value, 0, 1, tmp);
        esp_mqtt_client_publish(mqttHandle, (base + "/ph").c_str(), tmp, 0, qos, retain);
      }
      break;
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
            // Don't block here: this callback runs on the esp_mqtt_client
            // task, which has a small stack unsuited to a long TLS
            // download + flash write. Defer to loop() on the main task.
            Serial.printf("[OTA] New fw %s (have %s) — queued\n", ver, FW_VERSION);
            pendingOtaUrl = url;
          }
        }
      }
      break;
    }
    default: break;
  }
}

static void mqttStart() {
  if (!mqttCfg.enabled || !mqttCfg.uri.length()) return;
  if (!wifiSt.connected) return;

  if (mqttHandle) {
    esp_mqtt_client_stop(mqttHandle);
    esp_mqtt_client_destroy(mqttHandle);
    mqttHandle = nullptr;
    mqttConnected = false;
  }

  String clientId = "rs485node-" + chipSuffix();
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
  // No cert_pem set — platform returns a plain (non-TLS) MQTT/WebSocket URI

  Serial.printf("[MQTT] Starting — uri=%s user=%s\n",
    mqttCfg.uri.c_str(), mqttCfg.user.c_str());

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
 * WEB: PROVISIONING
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
    c.pub_period_ms = 2000;

    Serial.printf("[PROV] uri=%s\n", c.uri.c_str());
    Serial.printf("[PROV] user=%s\n", c.user.c_str());
    Serial.printf("[PROV] pass=%s (len=%d)\n", c.pass.length() ? "***" : "(empty)", c.pass.length());
    Serial.printf("[PROV] topic=%s\n", c.base_topic.c_str());

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

/**************************************************************
 * WEB: ROUTES
 **************************************************************/
static void setupRoutes_AP(){
  // Captive portal probes — all OS flavours redirect to ap.html
  auto redir = [](AsyncWebServerRequest *req){ req->redirect("/"); };
  server.on("/generate_204",               HTTP_ANY, redir);
  server.on("/hotspot-detect.html",        HTTP_ANY, redir);
  server.on("/connecttest.txt",            HTTP_ANY, redir);
  server.on("/ncsi.txt",                   HTTP_ANY, redir);
  server.on("/fwlink",                     HTTP_ANY, redir);
  server.on("/canonical.html",             HTTP_ANY, redir);
  server.on("/success.txt",               HTTP_ANY, redir);
  server.on("/library/test/success.html", HTTP_ANY, redir);
  server.on("/redirect",                   HTTP_ANY, redir);
  server.on("/ncc.txt",                    HTTP_ANY, redir);
  server.on("/chromehotstart.crx",         HTTP_ANY, redir);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(LittleFS, "/www/ap.html", "text/html");
  });

  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(LittleFS, "/www/style.css", "text/css");
  });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(LittleFS, "/www/app.js", "application/javascript");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    String hn = "rs485node-" + chipSuffix();
    String json = "{\"ok\":true,\"mode\":\"ap\",\"hostname\":\"" + hn + ".local\"}";
    req->send(200, "application/json", json);
  });

  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *req){
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
      req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
      return;
    }
    if (n < 0) {
      WiFi.scanNetworks(true); // start async scan
      req->send(200, "application/json", "{\"scanning\":true,\"networks\":[]}");
      return;
    }
    String json = "{\"scanning\":false,\"networks\":[";
    for (int i = 0; i < n; i++){
      if (i > 0) json += ",";
      String ssid = WiFi.SSID(i);
      ssid.replace("\"", "\\\"");
      json += "{\"ssid\":\"" + ssid + "\","
              "\"rssi\":" + String(WiFi.RSSI(i)) + ","
              "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]}";
    WiFi.scanDelete();
    req->send(200, "application/json", json);
  });

  server.on("/api/wifi", HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      StaticJsonDocument<256> in;
      StaticJsonDocument<128> out;
      if (deserializeJson(in, data, len)){
        out["ok"] = false; out["err"] = "bad_json";
        sendJson(req, out); return;
      }
      String ssid = in["ssid"] | ""; ssid.trim();
      String pass = in["pass"] | "";
      if (!ssid.length()){
        out["ok"] = false; out["err"] = "ssid_required";
        sendJson(req, out); return;
      }
      prefs.begin("wifi", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();
      out["ok"] = true; out["rebooting"] = true;
      sendJson(req, out);
      delay(400);
      ESP.restart();
    }
  );

  server.onNotFound([](AsyncWebServerRequest *req){ req->redirect("/"); });

  server.begin();
  Serial.println("[AP] Web server started.");
}

static void setupRoutes_STA(){
  // Explicit authenticated routes for the dashboard assets — a serveStatic()
  // + setFilter(authOK) approach was tried here first, but ESPAsyncWebServer's
  // filter just excludes the handler from matching on failure rather than
  // sending a 401 challenge, so unauthenticated requests silently fell
  // through to onNotFound() ("Not Found") instead of prompting for
  // credentials. Explicit routes that call requireAuthOr401() actually
  // trigger the browser's login dialog.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    req->send(LittleFS, "/www/index.html", "text/html");
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    req->send(LittleFS, "/www/style.css", "text/css");
  });
  server.on("/app.js", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    req->send(LittleFS, "/www/app.js", "application/javascript");
  });

  server.on("/provision", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    req->send_P(200, "text/html", PROVISION_HTML);
  });

  server.on("/api/provision", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
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
    req->send(404, "text/plain", "Not found");
  });

  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    StaticJsonDocument<640> doc;
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

    doc["sensor_type"] = sensorTypeStr(sensorType);
    doc["hostname"] = g_hostname.length() ? (g_hostname + ".local") : "";
    sendJson(req, doc);
  });

  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    StaticJsonDocument<448> doc;
    doc["ok"] = true;
    doc["sensor_type"] = sensorTypeStr(sensorType);
    doc["temp_c"] = sens.temp_c;
    doc["humidity_percent"] = sens.humidity_percent;
    doc["th_valid"] = sens.th_valid;
    doc["co2_ppm"] = sens.co2_ppm;
    doc["co2_temp_c"] = sens.co2_temp_c;
    doc["co2_humidity_percent"] = sens.co2_humidity_percent;
    doc["co2_valid"] = sens.co2_valid;
    doc["ec_tds_value"] = sens.ec_tds_value;
    doc["ec_valid"] = sens.ec_valid;
    doc["ph_value"] = sens.ph_value;
    doc["ph_temp_c"] = sens.ph_temp_c;
    doc["ph_valid"] = sens.ph_valid;
    sendJson(req, doc);
  });

  server.on("/api/sensor", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    StaticJsonDocument<128> doc;
    doc["ok"] = true;
    doc["type"] = sensorTypeStr(sensorType);
    sendJson(req, doc);
  });

  server.on("/api/sensor", HTTP_POST,
    [](AsyncWebServerRequest *req){},
    NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t, size_t){
      if (!requireAuthOr401(req)) return;
      StaticJsonDocument<128> in;
      StaticJsonDocument<128> out;
      if (deserializeJson(in, data, len)){
        out["ok"] = false; out["err"] = "bad_json";
        sendJson(req, out); return;
      }
      String type = in["type"] | "";
      if (type != "th" && type != "co2" && type != "ec" && type != "ph"){
        out["ok"] = false; out["err"] = "invalid_type";
        sendJson(req, out); return;
      }
      saveSensorType(sensorTypeFromStr(type));
      out["ok"] = true; out["type"] = sensorTypeStr(sensorType);
      sendJson(req, out);
    }
  );

  server.on("/api/clear-provision", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    prefs.begin("prov", false);
    prefs.putBool("done", false);
    prefs.end();
    prefs.begin("mqtt", false);
    prefs.clear();
    prefs.end();
    req->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/settings/mqtt", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!requireAuthOr401(req)) return;
    StaticJsonDocument<512> doc;
    doc["ok"] = true;
    doc["enabled"] = mqttCfg.enabled;
    doc["connected"] = mqttSt.connected;
    doc["uri"] = mqttCfg.uri;
    doc["user"] = mqttCfg.user;
    doc["base_topic"] = mqttCfg.base_topic;
    doc["retain"] = mqttCfg.retain;
    doc["pub_period_ms"] = mqttCfg.pub_period_ms;
    doc["mqtt_err"] = mqttSt.err;

    bool provDone = false;
    prefs.begin("prov", true);
    provDone = prefs.getBool("done", false);
    prefs.end();
    doc["provisioned"] = provDone;

    sendJson(req, doc);
  });

  server.begin();
  Serial.println("[STA] Web server started.");
}

/**************************************************************
 * FACTORY RESET (BOOT button, held 5s at runtime)
 **************************************************************/
static uint32_t btnHeldSince  = 0;
static bool     btnResetFired = false;

static void factoryResetWifi() {
  Serial.println("[BTN] Held 5s — factory reset: clearing WiFi/MQTT/provisioning config.");
  prefs.begin("wifi", false); prefs.clear(); prefs.end();
  prefs.begin("mqtt", false); prefs.clear(); prefs.end();
  prefs.begin("prov", false); prefs.clear(); prefs.end();
  delay(200);
  ESP.restart();
}

static void pollFactoryResetButton() {
  bool pressed = (digitalRead(BOOT_BTN_PIN) == LOW);
  uint32_t now = millis();
  if (pressed) {
    if (btnHeldSince == 0) btnHeldSince = now;
    if (!btnResetFired && (now - btnHeldSince) >= FACTORY_RESET_HOLD_MS) {
      btnResetFired = true;
      factoryResetWifi();
    }
  } else {
    btnHeldSince  = 0;
    btnResetFired = false;
  }
}

/**************************************************************
 * SETUP / LOOP
 **************************************************************/
void setup(){
  Serial.begin(115200);
  delay(200);
  Serial.println("=== rs485Node boot ===");

  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW);

  loadMqtt();
  loadSensorType();

  Serial2.begin(sensorBaud(sensorType), SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  modbusNode.begin(SENSOR_SLAVE_ID, Serial2);
  modbusNode.preTransmission(preTransmission);
  modbusNode.postTransmission(postTransmission);

  WiFi.onEvent(onWiFiEvent);

  if (!LittleFS.begin(true)){
    Serial.println("LittleFS mount failed");
  }

  g_hostname = "rs485node-" + chipSuffix();

  startSTA();
  uint32_t t0 = millis();
  while (millis() - t0 < 8000){
    if (WiFi.status() == WL_CONNECTED) break;
    delay(50);
  }

  // A saved SSID means this device was already provisioned. Even if the
  // initial connect attempt times out (e.g. router still booting after a
  // power outage), stay in STA mode and let WiFi.setAutoReconnect(true)
  // (set in startSTA()) keep retrying in the background — never treat a
  // temporarily unreachable router as "unprovisioned" and drop to AP mode.
  // The only explicit way back to AP mode is the runtime factory-reset
  // button (hold BOOT 5s), which clears the saved SSID and reboots.
  prefs.begin("wifi", true);
  bool hasSavedSsid = prefs.getString("ssid", "").length() > 0;
  prefs.end();

  if (WiFi.status() != WL_CONNECTED && !hasSavedSsid){
    startAP();
    setupRoutes_AP();
  } else {
    if (WiFi.status() == WL_CONNECTED) startMDNSOnce();
    setupRoutes_STA();
  }
}

void loop(){
  static uint32_t lastSensor=0, lastMqtt=0;
  uint32_t now = millis();

  pollFactoryResetButton();
  wifiTick();

  // Pending OTA — run on the main task (not the MQTT callback) so the
  // blocking TLS download + flash write has a normal stack to work with.
  if (pendingOtaUrl.length()) {
    String url = pendingOtaUrl;
    pendingOtaUrl = "";
    performOTA(url);
  }

  if (now - lastSensor >= TICK_SENSOR_MS){
    lastSensor = now;
    sensorTick();
  }

  if (now - lastMqtt >= TICK_MQTT_MS){
    lastMqtt = now;
    mqttTick(now);
  }
}
