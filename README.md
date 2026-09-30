# RP2040-Zero (Waveshare)

[中文文档](README.zh.md) | [English](README.md)

A board under the board-centric repo convention. **This directory is organized by the "board as root" rule**:

```
rp2040-zero/
├── README.md          # this file: all hardware info for this board
└── <project>/         # one directory per project built on this board (named by capability)
    ├── CMakeLists.txt / main/ / pico_sdk_import.cmake   # the RP2040-shaped build trio (pico-sdk)
    └── README.md      # project description + build/flashing commands
```

Key points of the convention:

- **Board directory name** = board name (kebab-case); the root README covers hardware only, never project content;
- **Each project directory builds standalone**: for RP2040, per its own SDK (official pico-sdk + CMake), it ships the
  build trio; with `PICO_SDK_PATH` configured, `cmake -B build && cmake --build build` produces the UF2;
- Projects share no code; when commonality is needed, copy first, and consider extracting a shared component only once things stabilize.

### Firmware baseline norms (mandatory fleet-wide)

Two baselines are mandatory for every MiBee firmware repo, and **every project** inside a
board repo must satisfy them:

1. **Watchdog: mandatory.** No naked main loops — on RP2040 that means enabling the
   on-chip hardware watchdog (`watchdog_enable`) and feeding it from the main loop;
2. **Web/API firmware upgrade (OTA): mandatory where the hardware allows.** This board
   has **no radio at all**, so web/API OTA is hardware-exempt — upgrades go through UF2
   (BOOTSEL drag-drop / picotool / SWD), and serialtap's 1200bps-touch soft-reboot into
   BOOTSEL keeps it hands-free. The norm binds again once a networking companion is added.

| Project | Watchdog | Web/API OTA |
|---------|----------|-------------|
| env-station | ✅ hardware watchdog (`watchdog_enable(8000, 1)`, 8 s) | N/A (no network; UF2/picotool/SWD is the path) |

---

## Board Overview

| Item | Value |
|------|-----|
| Module/chip | RP2040 — Arm Cortex-M0+ **dual-core** @133MHz, 264KB on-chip SRAM |
| Flash | 2MB external QSPI (**dedicated bus, not GPIOs, not broken out**) |
| Wireless | **None** (no radio at all — a pure USB device board; networking needs an external companion) |
| USB | **Native USB 1.1 (Type-C), no USB-UART bridge chip**; BOOTSEL download mode enumerates as the `RPI-RP2` mass-storage drive (drag-drop UF2 to flash) |
| Peripherals | 2×UART · 2×SPI · 2×I2C · 16×PWM · 4×12-bit ADC (GP26–29) + on-chip temp sensor · **2× PIO (8 state machines, software-defined timing)** |
| Onboard LED | **WS2812 RGB on GP16** (DIN; that pin is not broken out) |
| Buttons | BOOT (=BOOTSEL, enters UF2 download mode, **not a GPIO**), RESET |
| Debug | 3 SWD pads on the back (SWCLK/SWDIO/GND) — openocd + Picoprobe/Debug Probe for both flashing and debugging |
| Power | USB-C 5V input; onboard LDO outputs 3.3V; 5V/3V3/GND pads broken out (mind the LDO headroom when powering peripherals from 3V3) |
| Breakout | 23 castellated half-holes on the front three edges (left 8 + bottom 7 + right 8) + 10 on the back staggered row = 33 pads; **all 29 user GPIOs broken out** (GP16 yields to the WS2812) |
| Dimensions | 18.00 × 23.50 mm (USB-C on the top edge), 2.54mm pad pitch |

## Pinout Diagram (USB-C pointing up, front/component-side view; pin numbers match the official silkscreen)

```
                 ┌─ USB-C ─┐
        5V ◎┬───┘  [WS2812] ├───┬◎ GP0    ← SPI0 RX · I2C0 SDA · UART0 TX
       GND ◎│      (GP16)   │   ◎ GP1    ← SPI0 CSn · I2C0 SCL · UART0 RX
       3V3 ◎│               │   ◎ GP2    ← SPI0 SCK · I2C1 SDA
      GP29 ◎│ [BOOT] [RST]  │   ◎ GP3    ← SPI0 TX · I2C1 SCL
      GP28 ◎│               │   ◎ GP4    ← SPI0 RX · I2C0 SDA · UART1 TX
      GP27 ◎│    ┌─────┐    │   ◎ GP5    ← SPI0 CSn · I2C0 SCL · UART1 RX
      GP26 ◎│    │ RP2 │    │   ◎ GP6    ← SPI0 SCK · I2C1 SDA
      GP15 ◎│    │ 040 │    │   ◎ GP7    ← SPI0 TX · I2C1 SCL
           └───────────────┘
            bottom edge (left→right): GP14 · GP13 · GP12 · GP11 · GP10 · GP9 · GP8

  Back (silkscreen side): a staggered row of half-holes sits in the gaps of the
  right/bottom edges, from the USB end:
  GND · GP25 · GP24 · GP23 · GP22 · GP21 · GP20 · GP19 · GP18 · GP17
  (GP21/GP20 double as I2C0; the back also carries the 3 SWD pads;
  check the back silkscreen before using)
```

Key points:

- **Left row** (downward from the USB end): `5V, GND, 3V3, GP29, GP28, GP27, GP26, GP15`;
- **Right row** (downward from the USB end): `GP0–GP7`;
- **Bottom edge** (left→right): `GP14, GP13, GP12, GP11, GP10, GP9, GP8`;
- **Back staggered row**: `GND · GP25 · GP24 · GP23 · GP22 · GP21 · GP20 · GP19 · GP18 · GP17` (when soldering headers, remember to solder from both front and back);
- **The function annotations are the official default mapping, not a fixed mux table**: RP2040 peripherals route through a full matrix — UART/SPI/I2C can be remapped to almost any pin, and the 8 PIO state machines across 2 blocks can synthesize arbitrary timing. This is the biggest difference from ESP's fixed alternate-function tables;
- GP26–29 = ADC0–3 (12-bit, referenced to the 3V3 rail) + on-chip temperature sensor; GP16 = WS2812 DIN (not broken out);
- The QSPI flash lines are not GPIOs; the BOOT button is BOOTSEL (a hardware download-select), so there are no ESP-style strapping traps.

## Caveats

- **No USB-UART bridge, and not USB-Serial-JTAG either**: the serial port is the RP2040's native USB 1.1 —
  **you only get a serial port once the firmware enables CDC** (pico-sdk `stdio_usb` / TinyUSB CDC);
  **in BOOTSEL mode it is a USB drive, not a serial port** (`RPI-RP2`, drag a UF2 to flash) —
  serialtap's capture/passthrough chain depends on the firmware having CDC on.
- **Download mode is completely different from ESP**: there is no esptool-style serial reset protocol —
  hold BOOT while plugging USB (or press RESET while running) to enter BOOTSEL; real flashing goes through
  picotool or a UF2 copy. Firmware can also soft-reboot into BOOTSEL via the bootrom API (hands-free,
  matching the esptool auto-reset experience).
- **GPIO levels are 3.3V, not 5V tolerant**; there are no ESP-style strapping pins (BOOTSEL is a hardware
  select line, not a GPIO).
- GP16 is taken by the WS2812; the QSPI flash lines are not GPIOs — the remaining 29 pins
  (GP0–15, GP17–29) are all usable.
- No dedicated ADC_VREF pad (the Pico has one, this board doesn't): the ADC reference is the 3V3 rail, so
  supply ripple lands directly in ADC readings — weigh that for precision sampling.

## Serialtap (middleware) integration points (verified on hardware, 2026-09-27)

- With USB CDC enabled, the board enumerates as an ordinary CDC serial device (VID 2e8a,
  pico-sdk default PID 000a; **the USB serial number = the board flash unique ID in
  UPPERCASE hex** — stable across ports and firmware, ideal anchor for the semantic name);
  serialtap has a built-in `rp2040-cdc` naming family (0003/0005/000a); this unit is
  named `rp2040-zero`;
- **⚠ DTR adaptation (a trap we hit)**: pico-sdk's stdio_usb gates all output on DTR
  ("is the host listening") and **silently drops everything when DTR is not asserted**,
  while serialtap by default releases DTR/RTS after open (protecting CH340/ESP boards).
  The fix is serialtap's new `dtr_hold` config: matched devices keep DTR+RTS asserted
  after open — verified with #ENV telemetry flowing into the log;
- **Two verified flashing routes**: BOOTSEL + UF2 drag-drop (RPI-RP2 drive) /
  **1200bps touch soft-reboot into BOOTSEL** (on by default in pico-sdk: opening the
  port at 1200 baud triggers it, works after serialtap yields the port — no button
  press); the picotool channel for `serialtap flash` is still on the capability-map
  to-do (this board is its test bench);
- The WS2812 (GP16) can serve as a "device status LED", following the S3 board pattern.

## Toolchain

Official **pico-sdk (C/C++) + CMake + arm-none-eabi-gcc** (workspace convention:
RP2040 uses its own SDK). Installed on this workstation (2026-09-27): SDK at
`~/pico-sdk` (with the `lib/tinyusb` submodule) and the cross-compiler at
`~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1` (xPack GCC 15.2.1; there is no
package-manager install path, so add both to PATH/`PICO_SDK_PATH` when building —
see the verified commands in env-station's README). Alternatives: MicroPython /
Arduino — for MicroPython, the build-trio convention would be redefined to match.
Three flashing routes: BOOTSEL + UF2 drag-drop / `picotool load` / openocd (SWD).

## Project Index

| Project | Description |
|---------|-------------|
| [env-station](env-station/README.md) | Environment mini-station (RP2040 version, isomorphic to the [S3 board's same-named project](../esp32-s3-zero/env-station/README.md)): SSD1306 OLED (GP12/13 I2C0) + DHT11/22 (GP27) + TEMT6000 light (GP28); `#ENV` JSON-line telemetry over USB CDC; WS2812 status LED (doubling as the blink baseline) + hardware watchdog |
