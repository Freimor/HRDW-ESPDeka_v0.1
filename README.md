# HRDW-ESPDeka_v0.1

Offgrid LORA communicator firmware for the **Guition JC4880P443C** platform
(ESP32-P4 main core + ESP32-C6 radio co-processor), built with **ESP-IDF v5.x**
and **FreeRTOS**. See [`PROJECT_BRIEF.md`](PROJECT_BRIEF.md) for the mission,
philosophy and roadmap.

---

## Roadmap stage 1.1 (this build)

> Bring up the RTOS and a basic **chat application**. Write a message to a
> virtual contact; messages are logged.

Implemented:

- FreeRTOS bring-up via ESP-IDF (statically allocated tasks/queues/mutex — no `malloc` in application code).
- **`chat_core`** — deterministic chat engine: rolling message history, a
  "virtual contact" task that echoes replies, and structured logging.
- **`bsp`** — display stack for the JC4880P443C: ST7701S over MIPI-DSI (480×800),
  PWM backlight, GT911 capacitive touch, and LVGL via `esp_lvgl_port`.
- **`chat_ui`** — LVGL messenger screen: scrollable bubbles, text field, send
  button and an on-screen keyboard.
- **`expand_io`** — centralized Expand IO pin map (foundation for GNSS/LORA in stage 1.2).

**Success check:** on boot the display shows a welcome message from *Virtual
Contact*. Type a message on the on-screen keyboard and press **Send** → your
bubble appears (right), followed shortly by an `Echo: …` reply (left). Every
message is also printed on the serial monitor by the `chat_core` tag.

> Note: the on-screen keyboard and fonts are currently **ASCII/English** only.
> Cyrillic input (custom keymap + font) is planned as a later enhancement.

---

## Project layout

```
├── CMakeLists.txt            # top-level project
├── sdkconfig.defaults        # target esp32p4, 16MB flash, PSRAM, LVGL options
├── partitions.csv            # 16MB layout (single factory app for 1.1)
├── firmware/
│   └── lora_coprocessor/     # ESP-IDF project for ESP32-C3 + LR1121 module
├── main/                     # app_main entry point
└── components/
    ├── expand_io/            # centralized Expand IO pin map (HAL foundation)
    ├── chat_core/            # chat engine (pure logic + FreeRTOS)
    ├── bsp/                  # display + touch + LVGL board support
    └── chat_ui/              # LVGL chat interface
```

Managed components (`esp_lvgl_port`, `esp_lcd_st7701`, `esp_lcd_touch_gt911`
and their dependencies incl. LVGL 9) are declared in
`components/bsp/idf_component.yml` and downloaded automatically on first build.

### LORA coprocessor (ESP32-C3 module)

The **P-3 V1.6** LR1121 module ships with ExpressLRS firmware. ESPDeka replaces it
with a custom UART coprocessor — see
[`firmware/lora_coprocessor/README.md`](firmware/lora_coprocessor/README.md).

---

## Prerequisites: install ESP-IDF v5.x (Windows)

The ESP32-P4 requires **ESP-IDF ≥ 5.3**. Two options:

### Option A — Official Windows installer (recommended)

1. Download the **ESP-IDF Windows Installer** (Offline, v5.3.x or newer) from
   <https://dl.espressif.com/dl/esp-idf/>.
2. Run it and let it install the toolchain, Python and Git. When finished it
   creates two shortcuts: **"ESP-IDF PowerShell"** and **"ESP-IDF CMD"**.
3. Open **"ESP-IDF PowerShell"** — the environment (`idf.py`, compiler, Python)
   is exported automatically inside that shell.

### Option B — Manual (git clone)

```powershell
mkdir $HOME\esp
cd $HOME\esp
git clone -b v5.3.5 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
.\install.ps1 esp32p4
```

Then, in every new terminal, activate the environment before building:

```powershell
. $HOME\esp\esp-idf\export.ps1
```

---

## Build, flash & monitor

From the repository root, inside an ESP-IDF shell:

```powershell
# One-time: select the target (creates sdkconfig from sdkconfig.defaults)
idf.py set-target esp32p4

# Build (first build also fetches the managed LVGL / panel / touch components)
idf.py build

# Flash and open the serial monitor (replace COMx with your USB-C port)
idf.py -p COMx flash monitor
```

Exit the monitor with `Ctrl+]`.

Optional configuration tweaks: `idf.py menuconfig`.

---

## Troubleshooting

- **`Missing dependencies for SOCKS support` on first build.** The ESP-IDF
  component manager honours the Windows system proxy. If it points to a local
  SOCKS proxy, either bypass it for the build (`$env:NO_PROXY = "*"` in the
  ESP-IDF shell before `idf.py build`) or install `pysocks` into the IDF venv.
- **`requires chip revision in range [v3.1 - v3.99]` on flash.** Early
  ESP32-P4 silicon (this board reports **rev v1.3**) is not covered by the IDF
  5.5 production default. The project already sets
  `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` and `CONFIG_ESP32P4_REV_MIN_100=y` in
  `sdkconfig.defaults`; delete `sdkconfig` and rebuild if you changed targets.
- **`esp_lcd_panel_swap_xy(...): swap_xy is not supported by this panel`.**
  Benign log from `esp_lvgl_port`: the ST7701 DSI panel has no hardware
  rotation. The UI runs in native portrait; ignore this message.

## Coding standards

Firmware follows the rules in [`.cursorrules`](.cursorrules): MISRA C:2012 /
BARR-C style, exact-width integer types, static allocation, centralized Expand
IO pins behind a HAL, and Doxygen file headers on new files. Any unavoidable
deviation (e.g. framework-owned allocation in LVGL/`esp_lvgl_port`, or the
panel register table) is marked with a `// MISRA DEVIATION:` comment.
