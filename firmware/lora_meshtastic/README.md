# Meshtastic for ESPDeka P-3 V1.6 (ESP32-C3 + LR1121)

Custom Meshtastic board variant for the **P-3 V1.6** module (ex-ExpressLRS **Generic C3 LR1121**).
Replaces the ESPDeka AT coprocessor firmware when you need a full Meshtastic mesh node.

## Hardware

| Function | GPIO | Connector |
|----------|------|-----------|
| LR1121 SPI | 4–7, 1–3, 2 | on-module |
| WS2812 LED | 8 | on-module |
| Console UART | TX=21, RX=20 | left pads (USB-UART) |
| Host UART (P4) | TX=19, RX=18 | RX2/TX2 top pads |

**Antenna:** connect 868 MHz SMA before transmitting.

## Build

Requires ESP-IDF Python 3.11 (`idf5.5_py3.11_env`). First build downloads Meshtastic + PlatformIO deps (~15–30 min).

```powershell
cd firmware\lora_meshtastic
.\build.ps1
```

Retry if GitHub timeouts occur during library download.

## Flash

```powershell
.\flash.ps1 -Port COM5
```

Boot mode if needed: hold **BOOT**, tap **EN**, release **BOOT**.

## After flash

### Phone (standalone)

1. Install Meshtastic app.
2. Pair over **Bluetooth** with the module powered.
3. Set region **EU_868** (or your band), channel, PSK.

### CLI check (USB-UART on left pads)

```powershell
pip install meshtastic
meshtastic --port COM5 --info
```

### ESP32-P4 host (roadmap 1.3)

Enable Serial Module for TEXTMSG bridge on host UART:

```powershell
meshtastic --port COM5 --set serial.enabled true
meshtastic --port COM5 --set serial.mode TEXTMSG
meshtastic --port COM5 --set serial.txd 19
meshtastic --port COM5 --set serial.rxd 18
meshtastic --port COM5 --set serial.baud 38400
```

Wire P4 **IO52 → C3 RX (18)**, **IO51 ← C3 TX (19)**, common GND.

## Restore ESPDeka coprocessor

```powershell
cd ..\lora_coprocessor
idf.py -p COM5 -b 115200 flash
```

## Files

| Path | Purpose |
|------|---------|
| `espdeka_p3_v16/variant.h` | Pin map (source of truth) |
| `espdeka_p3_v16/platformio.ini` | PlatformIO env `espdeka-p3-v16-lr1121` |
| `build.ps1` / `flash.ps1` | Build and upload helpers |
| `firmware-master/` | Downloaded upstream (gitignored) |

## Notes

- LR1121 support in Meshtastic is **experimental**; use 868 MHz first.
- This overwrites factory ExpressLRS and ESPDeka copro v0.2 firmware.
- Meshtastic uses **PlatformIO/Arduino**, not ESP-IDF.
