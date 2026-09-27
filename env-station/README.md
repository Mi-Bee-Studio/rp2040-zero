# env-station (rp2040-zero) — temperature/humidity + light + OLED mini station

[中文文档](README.zh.md) | [English](README.md)

The RP2040-Zero sibling of
[esp32-s3-zero/env-station](../../esp32-s3-zero/env-station/README.md):
same name, same structure (`main.c` + `dht` + `ssd1306`, identical interfaces,
same `#ENV` line protocol), no radio — telemetry goes over USB CDC
(collected by serialtap).

## Wiring (as built)

| Peripheral | Pins | Notes |
|------|------|------|
| SSD1306 128x64 OLED | **SDA=GP12, SCL=GP13**, 3V3, GND | I2C0, address 0x3C; GP12/13 are the I2C0 hardware-native pair (even=SDA, odd=SCL); if reversed, just swap the two defines in `main.h` |
| DHT11/DHT22 | **DATA=GP27**, 3V3, GND | 1-wire; modules carry a 4.7~10k pull-up (add one for a bare sensor); model auto-detected |
| TEMT6000 light | **OUT=GP28**, 3V3, GND | Analog → ADC2; converted at 10k load ≈ 9.1mV/lux, full scale ≈ 360lux (maxed-out readings in direct sun are saturation, not a fault) |
| Onboard WS2812 | GP16 (on-board trace, no wiring) | Status LED: green breath = OK, red 2Hz blink = DHT failing |

## Build

Requires pico-sdk (with the `lib/tinyusb` submodule) + arm-none-eabi-gcc + CMake ≥3.13.

Verified on this workstation (2026-09-27, Git Bash; toolchain at `~/pico-sdk` and
`~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1`):

```bash
export PICO_SDK_PATH=~/pico-sdk
export PATH=~/toolchains/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin:/c/Espressif/tools/ninja/1.12.1:$PATH
cmake -B build -S . -G Ninja
cmake --build build
# artifact: build/main/env-station.uf2 (zero warnings with -Wall -Wextra)
```

Other environments (or `PICO_SDK_FETCH_FROM_GIT=1` to fetch the SDK automatically):

```bash
export PICO_SDK_PATH=~/pico-sdk    # or PICO_SDK_FETCH_FROM_GIT=1
cmake -B build -S . -G Ninja
cmake --build build
# artifact: build/main/env-station.uf2
```

## Flash

- **BOOTSEL**: hold BOOT while plugging USB → the `RPI-RP2` drive appears →
  drag `env-station.uf2` onto it;
- **1200bps touch** (hands-free while firmware runs): opening the port at 1200 baud
  soft-reboots into BOOTSEL (pico-sdk default). Under serialtap, `serialtap pause
  '^rp2040-zero$'` first to yield the port;
- **picotool**: `picotool load build/main/env-station.uf2 -fx`.

**serialtap capture note**: this firmware uses pico-sdk stdio_usb, which gates output
on DTR ("host listening") — the device must be listed in serialtap's `dtr_hold` config
(keep DTR+RTS after open), otherwise the firmware stays silent (full story in the board
README's integration section).

## Running

- Cadence: DHT every 2.5s, light every 0.5s, OLED refreshed at 4Hz, status LED
  every 250ms;
- **#ENV telemetry** (every 2.5s, on a successful read):

  ```
  #ENV {"seq":12,"t":24.6,"rh":58.1,"lux":312}
  ```

  Same JSON line as the S3 board (`lux` is the extra field on this board);
  the board applies no calibration — platform-side calibration is authoritative
  (see homepulse/docs/wfp-protocol.md).
- **Periodic status line** (every 60s): `[env] status: seq=.. dht_errs=..
  model=DHT11 oled=0x3C oled_errs=.. lux_mv=.. ss=on` — the boot banner prints
  once and may land in a serialtap pause window; this keeps peripheral state
  remotely observable at all times;
- **Model auto-detection (range fallback)**: decode as DHT22 first; if the
  result is out of physical range (rh>100 or out-of-range temperature) fall
  back to DHT11 — the old "decimal bytes nonzero" heuristic misfires when a
  DHT11 sends nonzero tenths (it once decoded this board's DHT11 as
  rh=998%/t=922°C; fixed 2026-09-27, mirrored to the S3 board's copy);
- **Diagnostics**: `[env] DHT read fail: timeout (errs=3)`;
- **Status LED tri-state**: green breath = all good / red 2Hz blink = DHT
  failing / orange 1Hz blink = OLED failing (the screen can't report its own
  failure — the LED is the only channel);
- **OLED**: see "UI & screensaver" — the DHT-error state shows big `DHT ERR`
  plus a small error-code line and a right-aligned light status (layout
  guaranteed non-overlapping and on-screen);
- **Watchdog**: hardware 8s, fed by the main loop — a hang resets the board.

## Troubleshooting

| Symptom | Fix |
|------|------|
| OLED stays black | Check address (usually 0x3C, sometimes 0x3D) and SDA/SCL order; for a 128x32 panel change `ssd1306.c` init to `0xA8,0x1F` and `0xDA,0x02` |
| DHT always times out | Check pull-up and supply; watch the errs counter on `[env]` lines; a dead DHT is usually stuck low |
| LUX pinned at max | Normal saturation (range ≈ 360lux); for a wide dynamic range use a digital sensor like BH1750 |
| No serial output | Firmware CDC appears ~1s after enumeration; make sure the tool opened the new COM port (in BOOTSEL mode the board is a USB drive, not a serial port) |
