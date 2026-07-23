# RS485 Node

ESP32-S3 firmware for the [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/wiki/ESP32-S3-RS485-CAN) DIN-rail module, reading Modbus RTU sensors over RS485 and integrating with the [IoT Platform](https://iot.unitani.com).

---

## Hardware

**Board:** Waveshare ESP32-S3-RS485-CAN (ESP32-S3R8, 8MB PSRAM, 16MB flash)

| Feature | GPIO | Notes |
|---|---|---|
| RS485 TX | GPIO17 | Onboard isolated transceiver |
| RS485 RX | GPIO18 | Onboard isolated transceiver |
| RS485 DE/RE | GPIO21 | Direction control |
| BOOT button | GPIO0 | Hold 5s at runtime → factory reset |

**Supported sensors** (only one wired to the bus at a time — see below):

| Sensor | Baud | Modbus slave ID | Registers |
|---|---|---|---|
| CWT-TH03S-H (temp/humidity probe) | 4800 | 1 (factory default) | `0x0000` humidity (0.1%RH), `0x0001` temperature (0.1°C, int16) |
| CWT CO2+Temp+Humidity integrated sensor | 4800 | 1 (factory default) | `0x0000` humidity, `0x0001` temperature, `0x0002` CO2 (ppm) |
| CWT-BL EC/TDS transmitter | 9600 | 1 (factory default) | `0x0000` calibration-solution value (unused), `0x0001` EC/TDS value (0.1 scale — unit/range depends on the transmitter's configured range) |
| CWT-BL pH transmitter | 9600 | 1 (factory default) | `0x0000` temperature (0.1°C, int16), `0x0001` pH (0.1 scale) |

All four sensors ship with the same default slave ID (1), so **no ID reassignment is needed** — the firmware only ever polls one sensor at a time, selected at runtime via the web dashboard. The TH03S/CO2 sensors default to 4800 baud while the EC/TDS and pH transmitters default to 9600 baud; the firmware automatically reconfigures the RS485 UART's baud rate when you switch between the two groups on the dashboard.

---

## Features

- **Single-sensor-at-a-time Modbus polling** — pick the active sensor from a dropdown on the dashboard (`/api/sensor`); the firmware polls only that sensor's register map every 2s, no reboot required to switch
- Captive-portal Wi-Fi setup (AP mode on first boot)
- **Platform provisioning** — connects to `iot.unitani.com` using your topic and API key; MQTT credentials are configured automatically
- Telemetry published to MQTT every 2s (`<base>/telemetry`, plus per-field topics)
- Basic Auth on all web UI routes (`admin` / `rs485node`)
- Self-healing Wi-Fi: retries STA indefinitely on connection loss, never falls back to AP mode once provisioned
- **Factory reset button** — hold the onboard BOOT button (GPIO0) for 5 seconds at runtime to clear saved WiFi/MQTT/provisioning config and reboot into the AP setup portal
- **OTA updates** — subscribes to `<base>/ota/command` (MQTT, retained); on receiving `{"version":"x.y.z","url":"..."}` with a version different from the running firmware, downloads and flashes the `.bin` over TLS (deferred to the main loop to keep the MQTT client task's stack safe), then reboots

---

## Getting Started

### 1. Flash the firmware

```bash
cd rs485Node
pio run --target upload
pio run --target uploadfs   # upload LittleFS web files
```

### 2. Wire the sensor

Only wire **one** sensor to the RS485 terminal at a time:

| Sensor wire | RS485 terminal |
|---|---|
| Yellow/green (A+) | `A+` |
| Blue (B-) | `B-` |
| Brown (Power +) | External 5–30V supply (per sensor spec) |
| Black (Power -) | External supply GND, shared with module GND |

Power the module itself from the terminal block (7–36V DC) or USB-C.

### 3. Wi-Fi setup

On first boot the device creates an access point:

```
SSID: RS485Node-XXXXXX   (last 3 bytes of MAC, no password)
```

Connect to it and open any browser — the captive portal loads automatically. Enter your Wi-Fi SSID and password, then save. The device reboots and connects to your network.

After connecting, the device is reachable at:

```
http://rs485node-XXXXXX.local/
```

### 4. Connect to the platform

Open the dashboard and tap **MQTT** to open the provisioning modal.

Enter:
- **Topic** — the base topic you want, e.g. `myfarm/rs485-1`. This must match the Device Name on the platform dashboard at `iot.unitani.com`.
- **API Key** — from **Settings → API Keys** on the platform dashboard.

Tap **Connect**. The device calls the platform API, receives MQTT credentials, and connects automatically. No manual MQTT configuration is needed.

---

## Web UI

After connecting to your network, open `http://rs485node-XXXXXX.local/` in a browser (Basic Auth: `admin` / `rs485node`).

| Page | Path | Description |
|---|---|---|
| Dashboard | `/` | Sensor selector, live readings, MQTT provisioning |

The dashboard shows an **Active sensor** dropdown at the top, labeled by sensor model rather than sensor type:

| Dropdown option | `/api/sensor` value |
|---|---|
| CWT TH03S | `th` |
| CWT CO2 | `co2` |
| CWT-BL EC/TDS | `ec` |
| CWT-BL pH | `ph` |

Changing the dropdown calls `/api/sensor` and the firmware starts polling that sensor's registers on the next 2s tick — no reboot required. The dropdown was chosen (over a two-way toggle) so it scales cleanly as more sensor types are added to the firmware later: extending it is just one new `<option>`, one new card grid, and one entry in `app.js`'s `SENSOR_CARD_IDS` map.

---

## MQTT Topics

Base topic is set during provisioning (e.g. `myfarm/rs485-1`).

| Topic | Direction | Payload |
|---|---|---|
| `<base>/telemetry` | Publish every 2s | JSON (see below) |
| `<base>/temp_c` | Publish (TH mode only) | Plain float |
| `<base>/humidity` | Publish (TH mode only) | Plain float |
| `<base>/co2_ppm` | Publish (CO2 mode only) | Plain integer |
| `<base>/ec_tds` | Publish (EC mode only) | Plain float |
| `<base>/ph` | Publish (pH mode only) | Plain float |
| `<base>/status` | Publish (retained, LWT) | `online` / `offline` |
| `<base>/ota/command` | Subscribe | `{"version":"x.y.z","url":"..."}` |

### Telemetry payload

**Temp/Humidity mode:**
```json
{
  "sensor_type": "th",
  "humidity_percent": 48.6,
  "temp_c": 27.4
}
```

**CO2 mode:**
```json
{
  "sensor_type": "co2",
  "co2_ppm": 812,
  "co2_temp_c": 26.1,
  "co2_humidity_percent": 45.9
}
```

**EC/TDS mode:**
```json
{
  "sensor_type": "ec",
  "ec_tds_value": 1413.0
}
```

**pH mode:**
```json
{
  "sensor_type": "ph",
  "ph_value": 7.02,
  "ph_temp_c": 25.1
}
```

---

## Dependencies

Managed by PlatformIO (`platformio.ini`):

| Library | Version |
|---|---|
| bblanchon/ArduinoJson | `^6.21.3` |
| 4-20ma/ModbusMaster | `^2.0.1` |
| esphome/ESPAsyncWebServer | latest |
| esphome/AsyncTCP | latest |

---

## Platform Integration

This node is integrated with the IoT SaaS platform at [iot.unitani.com](https://iot.unitani.com):

- `node_type` registered as `rs485node`
- Compatible with sensor-based automations (temperature/humidity/CO2/EC/TDS/pH thresholds)
