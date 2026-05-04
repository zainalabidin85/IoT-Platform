# IoT Platform — Node Firmware Collection

A collection of ESP32/ESP32-C3 firmware projects for MQTT-based IoT sensor and control nodes. Each node is a self-contained PlatformIO project that connects to the [IoT Platform](https://api-iot.unitani.com) and publishes data over MQTT.

---

## Nodes

| Folder | Board | Purpose |
|--------|-------|---------|
| `ecSensorNode` | ESP32-C3 SuperMini | Electrical conductivity (EC) sensor with calibration |
| `hydroNode` | ESP32-C3 SuperMini | Hydroponics node — EC + water level + temperature (DS18B20) |
| `NPKv2Node` | ESP32 Dev Board | Soil NPK/pH/conductivity/temp/humidity via RS485 Modbus sensor |
| `phSensorNode` | ESP32-C3 SuperMini | pH sensor with calibration, MQTT, and OTA updates |
| `switch4Node` | ESP32 Dev Board | 4-channel relay controller with dry-contact inputs |
| `switchNode` | ESP32 Dev Board | Single relay controller with dry-contact input |
| `tempNode` | ESP32 Dev Board | Temperature and humidity node (DHT22 or DS18B20) |
| `WaterLevelNode` | ESP32-C3 SuperMini | Water level sensor with calibration and OTA updates |

---

## Common Features

All nodes share these capabilities:

- **Wi-Fi provisioning** via captive portal — on first boot the device creates a Wi-Fi access point. Connect to it and open any browser to configure your Wi-Fi credentials.
- **Web UI** served from LittleFS — accessible at `http://<device-ip>/` after connecting to your network.
- **REST API** — JSON endpoints for reading sensor data and changing settings.
- **MQTT publishing** — publishes sensor readings to a configurable broker topic.
- **Platform provisioning** — registers with `https://api-iot.unitani.com` to obtain MQTT credentials automatically.
- **OTA firmware update** — supported on most nodes via MQTT `ota/command` topic.

---

## Requirements

### Tools

- [PlatformIO](https://platformio.org/) — install via VS Code extension or CLI
- Python 3.x (required by PlatformIO)

### Hardware

| Component | Used by |
|-----------|---------|
| ESP32-C3 SuperMini | ecSensorNode, hydroNode, phSensorNode, WaterLevelNode |
| ESP32 Dev Board (38-pin) | NPKv2Node, switch4Node, switchNode, tempNode |
| LCD 20×4 I2C (address 0x27) | ecSensorNode, hydroNode, phSensorNode, tempNode, WaterLevelNode |
| EC transmitter (0–5V, 12V powered) | ecSensorNode, hydroNode |
| Water level sensor (0–5V or 0–3.3V) | hydroNode, WaterLevelNode |
| DS18B20 temperature sensor | hydroNode, tempNode |
| DHT22 temperature & humidity sensor | tempNode (optional, selectable) |
| CWT-SOIL-NPKPHCTH-S + MAX485 module | NPKv2Node |
| 4-channel relay module | switch4Node |
| Single relay module | switchNode |

---

## Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/zainalabidin85/iotPlatform.git
cd iotPlatform
```

### 2. Open a node in PlatformIO

```bash
cd ecSensorNode   # replace with the node you want
pio run            # build
pio run -t upload  # flash to board
```

Or open the folder directly in VS Code with the PlatformIO extension installed.

### 3. Upload the filesystem (web UI files)

For nodes that include a `data/www/` folder (hydroNode, switch4Node, switchNode, WaterLevelNode):

```bash
pio run -t uploadfs
```

### 4. Provision Wi-Fi

1. Power on the device.
2. If no Wi-Fi credentials are stored, the device broadcasts an access point (SSID: `NodeAP-XXXXXX`).
3. Connect your phone or laptop to that network.
4. A captive portal page opens automatically — enter your Wi-Fi SSID and password.
5. The device reboots and connects to your network.

### 5. Access the web interface

Find the device IP from your router or serial monitor, then open:

```
http://<device-ip>/
```

---

## Node Details

### ecSensorNode

Measures electrical conductivity using a 0–5V EC transmitter module.

- **Buttons:** LIGHT/MODE · CAL/UP · DOWN/ENTER
- **API endpoints:** `/api/status` · `/api/ec` · `/api/cal`
- **MQTT config:** via web API at `/api/settings/mqtt`
- **Note:** This node is a compilable skeleton/blueprint — no OTA by design.

---

### hydroNode

Combined hydroponics monitoring node.

- **Sensors:** EC transmitter (0–5V) · water level (analog) · DS18B20 temperature (GPIO5)
- **Buttons:** LIGHT/MODE · CAL/UP · DOWN/ENTER
- **MQTT:** auto-disabled in AP mode to keep the UI responsive

---

### NPKv2Node

Reads NPK, pH, conductivity, temperature, and humidity from a CWT-SOIL-NPKPHCTH-S sensor over RS485 Modbus RTU.

- **Pins:** RX2=16 · TX2=17 · DE/RE=4 · BOOT button=0 (hold on power-up → force config portal)
- **Web UI:** protected with HTTP Basic Auth
- **Home Assistant:** supports MQTT discovery
- **Force config portal:** hold BOOT button while powering on

---

### phSensorNode

pH sensor node with 2-point calibration.

- **Buttons:** LIGHT/MODE · CAL/UP · DOWN/ENTER
- **API endpoints:** `/api/status` · `/api/ph` · `/api/cal`

---

### switch4Node

Controls 4 relays independently via MQTT or web UI.

- **Relay GPIO:** 32 · 33 · 25 · 26 (active HIGH)
- **Input GPIO:** 16 · 17 · 18 · 19 (dry contact to GND, INPUT_PULLUP)
- **Web UI auth:** `admin` / `switch4node`

---

### switchNode

Single relay controller.

- **Relay GPIO:** 16 (active HIGH)
- **Input GPIO:** 25 (dry contact to GND, INPUT_PULLUP)
- **Web UI auth:** `admin` / `switchnode`

---

### tempNode

Temperature and humidity monitoring node. Supports two sensor types — select at build time.

- **Sensor options:** DHT22 (temp + humidity) or DS18B20 (temp only) `Data PIN: 5`
- **Select sensor:** set `SENSOR_DHT22` or `SENSOR_DS18B20` define in `main.cpp`
- **LCD Display:** `SDA PIN: 21` · `SDL PIN: 22`
- **Buttons:** LIGHT/MODE · UP · DOWN/ENTER
-

---

### WaterLevelNode

Water level sensor node with calibration and OTA support.

- **Sensor:** 0–5V or 0–3.3V analog water level transmitter
- **Buttons:** LIGHT/MODE · CAL/UP · ENTER/DN
- **API endpoints:** `/api/status` · `/api/level` · `/api/cal`
- **OTA:** supported via MQTT `ota/command` topic

---

## MQTT Topic Convention

Each node uses a base topic configured via the web UI. Example:

```
home/sensor/ecsensornode-A1B2C3/data     ← sensor readings (JSON)
home/sensor/ecsensornode-A1B2C3/status   ← online/offline
home/switch/switch4node-A1B2C3/relay/1   ← relay command (ON/OFF)
```

The suffix `A1B2C3` is derived from the device MAC address.

---

## Platform Integration

Nodes that support platform provisioning connect to `https://api-iot.unitani.com` to:

1. Register the device and obtain an MQTT username/password.
2. Receive OTA firmware update commands.

Provisioning is triggered from the web UI — enter your platform **topic** and **API key**, then click Provision.

---

## License

See [LICENSE](LICENSE) for details.
