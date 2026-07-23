#!/usr/bin/env bash
# Safe app-only reflash for the on-site Rockk device (kc868a6Node).
# Writes ONLY the application partition at 0x10000 — never touches the
# NVS partition (0x9000-0xE000), so WiFi credentials, MQTT provisioning,
# and sensor calibration (ec_sl, ec_off, wt_vmin, wt_vmax) all survive.
#
# Usage: ./flash_rockk_1.0.7.sh /dev/ttyUSB0   (or COM3 on Windows, /dev/cu.usbserial-* on Mac)

set -euo pipefail

PORT="${1:-}"
if [ -z "$PORT" ]; then
  echo "Usage: $0 <serial-port>"
  echo "  e.g. $0 /dev/ttyUSB0"
  exit 1
fi

BIN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_FILE="$BIN_DIR/kc868a6Node_1.0.7.bin"
ESPTOOL="/home/zainal/.platformio/packages/tool-esptoolpy/esptool.py"

if [ ! -f "$BIN_FILE" ]; then
  echo "ERROR: $BIN_FILE not found."
  exit 1
fi

echo "=== Flashing app partition only (0x10000) ==="
echo "Port: $PORT"
echo "File: $BIN_FILE"
echo

python3 "$ESPTOOL" --chip esp32 --port "$PORT" --baud 460800 \
  write_flash 0x10000 "$BIN_FILE"

echo
echo "=== Done. Device should reboot into 1.0.7 automatically. ==="
echo "Calibration, WiFi, and MQTT provisioning were NOT touched (NVS untouched)."
echo "Check the OLED — bottom-right of the relay/input page should show: FW: 1.0.7"
