# AI Passport JBD Battery Monitor

<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

An offline, read-only JBD BMS monitor for the FoloToy AI Passport. The current
`v0.3` firmware connects directly to Gotion Gold battery pack B over Bluetooth
LE, without a phone relay.

## Current target

- Board: FoloToy AI Passport, ESP32-C3 rev 1.1, 8 MB Flash, no PSRAM
- Display: 240×320
- ESP-IDF: 5.5.3
- BMS name: `03HMB0GL0200CFG180600673`
- BMS address observed during validation: `20:25:12:29:25:63`
- GATT service: `0xFF00`
- Notify characteristic: `0xFF01`
- Write-without-response characteristic: `0xFF02`

## Features

- Scans, connects, discovers GATT characteristics, subscribes to notifications,
  polls verified read-only JBD commands, and reconnects after disconnects.
- Displays SOC, pack voltage, current, power, charge/discharge/idle state,
  remaining/rated capacity, RSSI, cell count, minimum/maximum cell voltage, and
  cell delta.
- Shows the AI Passport's own battery level independently in the top-right.
- Uses a Chinese LVGL font with verified glyph coverage for the 240×320 UI.
- Keeps JBD framing/checksum parsing and fixed-point display formatting separate
  from BLE and LVGL, with captured-frame regression tests.

## Safety scope

This application sends only the read queries confirmed from the supplied HCI
captures: `0x03` (basic status) and `0x04` (cell voltages). It does not write
configuration, switch MOSFETs, reset, calibrate, or change protection limits.

## Build

Activate ESP-IDF 5.5.3, then run:

```sh
idf.py build
idf.py merge-bin -o build/FoloToy-AI-Passport-full.bin
python tools/verify_firmware.py build
```

Flash only the merged image at offset `0x0`. Do not flash the app-only binary at
that offset.

```sh
esptool.py --chip esp32c3 --port COM3 write_flash 0x0 build/FoloToy-AI-Passport-full.bin
```

Do not run a whole-chip erase unless that separate destructive operation is
explicitly intended.

## Tested release

Download the validated merged firmware and debug bundle from the
[v0.3 release](https://github.com/qq305514253/ai-passport-battery-monitor/releases/tag/v0.3).

- Full firmware SHA-256:
  `C8E07A0BB8F00D1BF3D57C2E02D15199DE8BF113BF17F2116C7631C40FF871BC`
- Device validation: ESP32-C3 boot, display/LVGL, buttons, onboard battery,
  BLE scanning, direct BMS connection, SOC, 24-cell data, and RSSI.

## Limitations

- `v0.3` is a single-pack validation build configured for battery B.
- Battery A and simultaneous dual-pack monitoring are future work.
- A BMS Bluetooth module may accept only one central connection, so the original
  phone app and AI Passport may not be able to connect at the same time.

This repository is derived from the official
[FoloToy/ai-passport](https://github.com/FoloToy/ai-passport) project.
