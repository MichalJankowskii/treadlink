# TreadLink

[![BuyMeCoffee][buymecoffeebadge]][buymecoffee]

ESP32-S3 firmware that bridges a BLE treadmill (FTMS) to a Garmin watch by presenting as a Running Speed and Cadence (RSC) footpod sensor.

Built for the [Seeed Studio XIAO ESP32-S3](https://www.seeedstudio.com/XIAO-ESP32S3-p-5627.html).

## Install

Flash directly from your browser — no tools required:

**[Install TreadLink](https://michaljankowskii.github.io/treadlink/)** (Chrome/Edge, USB-C cable)

Or download the binaries from the [latest release](https://github.com/MichalJankowskii/treadlink/releases/latest).

## What it does

Most Garmin watches don't natively support BLE treadmill data (FTMS). TreadLink sits between your treadmill and watch:

```
Treadmill ──BLE FTMS──> TreadLink ──BLE RSC──> Garmin Watch
                            │
                        WiFi Web UI
```

- Connects to any BLE FTMS treadmill as a client
- Advertises as a footpod sensor that Garmin watches discover natively
- Converts speed, cadence, distance, and incline data in real time
- Provides a web UI for setup, monitoring, and treadmill control

## Features

- **Dual-role BLE** — simultaneous GATT client (treadmill) and GATT server (Garmin)
- **Auto-reconnect** — reconnects to saved treadmill with exponential backoff
- **Treadmill control** — set speed, incline, start/stop via FTMS Control Point from the web UI
- **WiFi web UI** — responsive single-page interface for configuration and live data
- **Speed unit support** — km/h and mph throughout
- **Simulated data** — diagnostics mode to test Garmin integration without a treadmill
- **Status LED** — blink patterns indicate connection state
- **Persistent config** — NVS storage for treadmill address, WiFi credentials, cadence settings

## Hardware

- Seeed Studio XIAO ESP32-S3
- USB-C cable for power and initial flashing
- Optional: 3D printed case (see [`case/`](case/))

## 3D Printed Case

A printable enclosure is provided in the `case/` directory:

- **`TreadLink.3mf`** — ready-to-print 3MF file for the XIAO ESP32-S3

## Building

Requires [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) (CLI) or the PlatformIO IDE extension for VS Code. On first build, PlatformIO downloads the pinned `espressif32` platform (ESP-IDF 5.5.3) and toolchain automatically. This takes several minutes.

### Build

```bash
git clone https://github.com/MichalJankowskii/treadlink.git
cd treadlink
pio run
```

The firmware image is written to `.pio/build/seeed_xiao_esp32s3/firmware.bin`.

### Flash to the device

1. Connect the XIAO ESP32-S3 over USB-C.
2. Find its serial port. It shows up with USB ID `303A:1001` (e.g. `COM3` on Windows, `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem*` on macOS):
   ```bash
   pio device list
   ```
3. Build and upload in one step:
   ```bash
   pio run -t upload --upload-port COM3
   ```
   `--upload-port` is optional when only one board is connected.

Uploading keeps saved settings (Wi-Fi, saved treadmill, Garmin pairing). To wipe them and start fresh, erase the flash first with `pio run -t erase`.

If the upload can't connect, put the board in bootloader mode. Hold **BOOT**, tap **RESET**, release **BOOT**, then upload again.

### Monitor serial output

```bash
pio device monitor
```

### Windows: user folder with non-ASCII characters

If your Windows user name contains non-ASCII characters (e.g. `ł`, `ö`), the ESP-IDF build fails with `UnicodeDecodeError` or `Kconfig ... not found` errors. The path to `~/.platformio` ends up garbled in generated build files. Point PlatformIO at a folder with an ASCII-only path, then delete the stale build directory and rebuild:

```powershell
$env:PLATFORMIO_CORE_DIR = "C:\pio"   # any ASCII-only path
$env:PYTHONUTF8 = "1"
Remove-Item -Recurse -Force .pio\build
pio run -t upload
```

PlatformIO downloads its packages into the new folder on first use. Junctions back to the old folder don't work, because CMake resolves them to the real path.

## Setup

1. Flash firmware via the [web installer](https://michaljankowskii.github.io/treadlink/) or PlatformIO
2. Connect to the **TreadLink** WiFi AP (password: `treadlink`)
3. Browse to `192.168.4.1`
4. Scan for your treadmill and connect
5. On your Garmin watch: Settings > Sensors > Add > search for "TreadLink"
6. Start a treadmill run on Garmin — speed and cadence data will flow automatically

To use your home WiFi instead of AP mode, configure WiFi STA credentials in the web UI settings.

## Web UI

The web interface provides:

- **Live data** — speed, cadence, distance, incline updated in real time
- **Treadmill scan & connect** — discover and pair BLE FTMS treadmills
- **Treadmill control** — speed/incline sliders, presets, start/stop (when supported)
- **Configuration** — WiFi mode, speed units, cadence estimation parameters
- **Simulate** — test Garmin integration without a treadmill
- **Log viewer** — real-time event log for debugging

## Architecture

```
components/
├── ble_common/          # Shared NimBLE stack init
├── ble_ftms_client/     # GATT client — connects to treadmill
├── ble_rsc_server/      # GATT server — advertises as footpod
├── data_bridge/         # FTMS → RSC unit conversion + cadence estimation
├── config_store/        # NVS persistent configuration
├── wifi_manager/        # WiFi AP/STA management
├── web_server/          # HTTP REST API + embedded HTML UI
└── led_status/          # Status LED blink patterns
main/
└── main.c               # Orchestration + callbacks
```

## LED Status

| Pattern | Meaning |
|---------|---------|
| Slow blink (1s) | No connection |
| Fast blink (100ms) | Scanning / connecting |
| Double blink | Treadmill connected, waiting for Garmin |
| Solid on | Both connected — data flowing |

## Support

If you find this useful, consider buying me a coffee:

[![BuyMeCoffee][buymecoffeebadge]][buymecoffee]

## License

MIT

[buymecoffee]: https://www.buymeacoffee.com/nathanmarlor
[buymecoffeebadge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge
