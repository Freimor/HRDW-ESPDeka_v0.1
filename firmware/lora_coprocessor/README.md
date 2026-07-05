# LORA Coprocessor Firmware (ESP32-C3 + LR1121)

Standalone ESP-IDF project that replaces the factory **ExpressLRS** firmware on the
**P-3 V1.6** class module (ESP32-C3 + Semtech LR1121) with an **ESPDeka UART
coprocessor**.

The **ESP32-P4** host (jc4880p443) talks to this module over UART — not to the LR1121
directly. The C3 will later own the LR1121 SPI link; stage **A** validates only the
host UART and AT protocol.

```
ESP32-P4 (ESPDeka)  ←UART→  ESP32-C3 (this firmware)  ←SPI→  LR1121
```

---

## Stage A (current) → v0.2.0 full AT + RGB LED

- Boot banner on the host UART
- **WS2812 RGB LED** on GPIO8 (GRB) with ExpressLRS-like patterns
- **LR1121 SPI probe** (reset + BUSY + GET_VERSION)
- Full **AT** dialect for the ESP32-P4 host (`lora_driver`)

### AT commands

| Command | Response / action |
|---------|-------------------|
| `AT` | `OK` (+ brief white LED flash) |
| `AT+GMR` | Firmware version string |
| `AT+HELP` | Lists all commands, ends with `OK` |
| `AT+RST` | `OK`, then reboot |
| `AT+STATUS?` | One-line telemetry snapshot |
| `AT+STREAM=1` or `/start` | `OK`, telemetry every 1 s, cyan LED |
| `AT+STREAM=0` or `/stop` | `OK`, stop telemetry |
| `AT+UART?` | `+UART:…` host port and baud |
| `AT+LR1121?` | `lr1121=ok` / `lr1121=fault` |
| `AT+LED=0` | Force LED off |
| `AT+LED=1` | Automatic LED (default) |
| `AT+LED=2` | Rainbow test pattern |
| `AT+LED?` | Current LED mode |

### RGB LED patterns (automatic mode, `AT+LED=1`)

| Pattern | Meaning |
|---------|---------|
| Rainbow fade (~1.5 s at boot) | Starting up |
| Slow green blink (500 ms) | Ready, waiting for host |
| Cyan blink (250 ms) | Telemetry stream active |
| Fast red blink (100 ms) | LR1121 not detected |
| Brief white flash | Successful `AT` command |

Telemetry example:

```text
LORA: ESPDeka-LORA-Copro v0.2.0 uart=ok lr1121=ok ver=0x01010000 stream=1 uptime=12s
```

---

## Hardware

| Item | Detail |
|------|--------|
| Module silk | **P-3 V1.6** |
| MCU | ESP32-C3 |
| RF | LR1121 (868 MHz + 2.4 GHz, dual antenna) |
| Factory FW | ExpressLRS receiver (CRSF) — **overwritten** by this build |
| Power | **5 V** on the `5V` pad (not only UART) |
| Bench flash UART | Left pads: **GND, EN, BOOT, RX, TX** → USB-UART @ 115200 |
| Host link to P4 | Expand IO **GPIO51/52** ↔ module **RX2/TX2** (use menuconfig UART1) |

### Pin map (`components/board_p3_v16/include/board_pins.h`)

LR1121 SPI (ExpressLRS *Generic C3 LR1121* layout):

| Signal | GPIO |
|--------|------|
| SCK | 6 |
| MISO | 5 |
| MOSI | 4 |
| CS | 7 |
| RESET | 2 |
| BUSY | 3 |
| DIO1 | 1 |
| RGB LED | 8 |

Host UART defaults:

| Connector | UART | MCU TX | MCU RX |
|-----------|------|--------|--------|
| Left `TX`/`RX` (flash / bench) | UART0 | 21 | 20 |
| Top `TX2`/`RX2` (P4 Expand IO) | UART1 | 19 | 18 |

> **Verify** RX2/TX2 GPIO mapping on your schematic before wiring to the jc4880p443.

---

## Build

### One-time: activate ESP-IDF in PowerShell

ESP-IDF is installed at `C:\Espressif\frameworks\esp-idf-v5.5.4\`. It is **not** on PATH
globally — each new shell must load it:

```powershell
cd firmware\lora_coprocessor
. .\activate-idf.ps1
```

Or manually (use the **Python 3.11** env — IDF 5.5 does not support 3.14 yet):

```powershell
$env:IDF_PYTHON_ENV_PATH = "C:\Espressif\python_env\idf5.5_py3.11_env"
. C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
```

After activation, `idf.py --version` should work. The Espressif **“ESP-IDF PowerShell”**
shortcut does the same thing automatically.

To make it permanent (optional): add to your PowerShell profile:

```powershell
. C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
```

### Compile

```powershell
cd firmware\lora_coprocessor
. .\activate-idf.ps1
idf.py set-target esp32c3
idf.py build
```

Optional: select host UART in menuconfig:

```powershell
idf.py menuconfig
# → ESPDeka LORA Coprocessor → Host UART
```

---

## Flash (USB-UART on COM port)

Wiring (3.3 V logic):

| USB-UART | Module pad |
|----------|------------|
| GND | GND |
| TX | RX |
| RX | TX |
| — | **5V** (module supply) |
| DTR → EN (optional auto-reset) | EN |
| RTS → GPIO0 (optional auto-reset) | BOOT |

Enter download mode: hold **BOOT**, tap **EN** (reset), release **BOOT**.

```powershell
idf.py -p COM5 flash monitor
```

Exit monitor: `Ctrl+]`.

### Quick bench test (Python)

```powershell
$env:PYTHONIOENCODING='utf-8'
python -c "import serial,time; s=serial.Serial('COM5',115200,timeout=1); time.sleep(1); print(s.read(999).decode(errors='replace')); s.write(b'AT\r\n'); time.sleep(0.5); print(s.read(999).decode(errors='replace')); s.write(b'AT+STREAM=1\r\n'); time.sleep(2); print(s.read(999).decode(errors='replace'))"
```

Expected: banner, `OK`, then once-per-second `LORA: ...` lines.

---

## Roadmap

| Stage | Goal |
|-------|------|
| **A** (done in v0.2) | UART + full AT + RGB LED + LR1121 probe |
| **B** | LoRa packet RX/TX, RSSI, region config |
| **C** | Meshtastic / mesh integration (ESPDeka roadmap 1.3) |

---

## Restoring ExpressLRS

Back up the factory image before the first flash if you may return the module to FPV use:

```powershell
esptool.py --chip esp32c3 -p COM5 read_flash 0 0x400000 elrs_backup.bin
```

Re-flash later with the [ExpressLRS Web Flasher](https://www.expresslrs.org/) — target
**Generic C3 LR1121** / **Unified_ESP32C3_LR1121_RX**.

---

## Related ESPDeka components

On the **ESP32-P4** firmware:

- `components/lora_driver` — host UART client (AT probe / telemetry)
- `components/expand_io` — GPIO51/52 pin map

After flashing this coprocessor, rebuild and flash the main ESPDeka app; use `/start` in
chat to see LORA telemetry alongside GNSS (roadmap 1.2).
