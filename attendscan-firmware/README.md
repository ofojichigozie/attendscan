# attendscan-firmware

ESP32-CAM firmware for the AttendScan QR-based attendance system. The board continuously captures frames, decodes any QR code found, and POSTs the embedded student registration number to a backend server over WiFi. All user-configurable values (WiFi credentials, server URL, scan interval, UI pins) live in `config.h` and are kept out of version control.

A single button arms and disarms the scanner. A single LED communicates system state through solid and blink patterns — no second button or LED needed.

---

## Requirements

| Tool | Notes |
|---|---|
| arduino-cli | Already installed |
| ESP32 core for Arduino | Installed via arduino-cli (see below) |
| `ESP32QRCodeReader` library | QR decode library (see below) |

---

## arduino-cli setup for ESP32-CAM

### 1 — Add the Espressif board index

The ESP32 core is not bundled with arduino-cli by default. Add it once:

```powershell
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

### 2 — Update the index and install the core

```powershell
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

This installs the full Espressif ESP32 Arduino core (includes `esp_camera`,
WiFi, HTTPClient, and the LEDC timer driver used by the camera).

### 3 — Install the ESP32QRCodeReader library

This library is not on the official Arduino registry and must be installed manually from GitHub. It uses PSRAM-aware quirc internally and runs the decoder on a dedicated FreeRTOS core, making it far more stable than calling quirc directly.

First, enable zip installs if you haven't already:

```powershell
arduino-cli config set library.enable_unsafe_install true
```

Download and fix the library structure (GitHub ZIPs have a known folder layout issue that arduino-cli rejects):

```powershell
Invoke-WebRequest -Uri https://github.com/alvarowolfx/ESP32QRCodeReader/archive/refs/heads/master.zip -OutFile ESP32QRCodeReader.zip
Expand-Archive -Path ESP32QRCodeReader.zip -DestinationPath ESP32QRCodeReader-extracted
Move-Item -Path ESP32QRCodeReader-extracted\ESP32QRCodeReader-master\include\* -Destination ESP32QRCodeReader-extracted\ESP32QRCodeReader-master\src\
Compress-Archive -Path ESP32QRCodeReader-extracted\ESP32QRCodeReader-master\* -DestinationPath ESP32QRCodeReader-fixed.zip
arduino-cli lib install --zip-path ESP32QRCodeReader-fixed.zip
```

### 4 — Verify core and library

```powershell
arduino-cli core list    # should show esp32:esp32
arduino-cli lib list     # should show ESP32QRCodeReader
```

---

## Configuration

Open `config.h` and fill in your values before compiling:

```cpp
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
#define SERVER_URL    "http://192.168.1.100:3000/scan"
```

`SERVER_URL` should be the LAN IP of the machine running the backend during local testing. Find it with `ipconfig` (Windows) or `ip a` (Linux/macOS). Switch to the production HTTPS URL when deploying against the real backend.

---

## Wiring

### ESP32-CAM ↔ USB-TTL adapter (for flashing and serial monitor)

| ESP32-CAM pin | USB-TTL pin | Notes |
|---|---|---|
| 5V | 5V (raw) | Power — use the raw USB 5V pin, not VCC |
| GND | GND | |
| U0TXD | RXD | Cross-connect TX → RX |
| U0RXD | TXD | Cross-connect RX → TX |
| IO0 | GND | **Flash mode only** — bridge before power-on, remove to run |

**Jumper cap on adapter:** bridge VCC → 3.3V. This keeps the data lines at 3.3V logic, which the ESP32 requires. Do **not** use the VCC pin to power the board — use the dedicated 5V pin instead.

### Button and LED

| Component | ESP32-CAM pin | Notes |
|---|---|---|
| Button (one leg) | GPIO 12 | |
| Button (other leg) | 3.3V | Active-high — add a 10kΩ pull-down resistor from GPIO12 to GND |
| LED anode (+) | GPIO 13 | Add a 330Ω series resistor between GPIO13 and the LED |
| LED cathode (−) | GND | |

> **Why GPIO 12 specifically?** Most GPIOs on the AI-Thinker ESP32-CAM are
> consumed by the camera. GPIO 12 and GPIO 13 are among the few left free.
> GPIO 12 must read LOW at boot (it controls the flash voltage strapping pin),
> so the button is wired active-high with a pull-down rather than the usual
> active-low with a pull-up. As long as the button is not held during power-on
> the board boots normally.

---

## LED behaviour

The LED communicates scanner state without any additional hardware:

| State | LED |
|---|---|
| Idle (scanner off) | Off |
| Ready (scanning active) | Solid on |
| QR code detected and POSTed | Fast blink 3× then back to solid |

Press the button once to arm the scanner (LED on). Press again to disarm (LED off). The deduplication cache also clears on each arm, so the first scan of a new session always fires even if the same code was the last one scanned in the previous session.

---

## Cloning

```bash
git clone https://github.com/ofojichigozie/attendscan.git
cd attendscan
cd attendscan-firmware
cp config.h.example config.h
# edit config.h with your credentials
```

After cloning the repository, navigate into the firmware folder before compiling or uploading. From there, you can use `.` as the sketch path instead of specifying the full folder name.

---

## Compiling

Find your board's port first:

```bash
arduino-cli board list
```

You should see something like `COM3` (Windows) or `/dev/ttyUSB0` (Linux).

Compile (replace `COM3` with your actual port):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32cam .
```

> The FQBN `esp32:esp32:esp32cam` targets the AI-Thinker ESP32-CAM module
> specifically in the ESP32 core installed on this machine. If `arduino-cli`
> reports `board esp32:esp32:ai_thinker not found`, use `esp32:esp32:esp32cam`
> instead. The generic `esp32:esp32:esp32` variant may compile but the camera
> pin map may not match.

---

## Uploading (flash mode)

1. **Bridge IO0 to GND** on the ESP32-CAM (see wiring table above).
2. **Plug in** the USB-TTL adapter — power the board now.
3. Upload:

```bash
arduino-cli upload --fqbn esp32:esp32:esp32cam --port COM3 .
```

4. Once upload completes, **remove the IO0-GND bridge**.
5. Press the **RST button** on the board (or power-cycle it) to boot normally.

---

## Monitoring serial output

```bash
arduino-cli monitor --port COM3 --config baudrate=115200
```

Exit the monitor with `Ctrl+C`.

Expected output on a normal session:

```
[SYS] AttendScan firmware starting...
[WiFi] connecting to MyNetwork......
[WiFi] connected — IP: 192.168.1.42
[SYS] idle — press button to start scanning

[SYS] scanner READY — waiting for QR codes
[QR] decoded: CSC/2021/001
[HTTP] POST 200 — reg: CSC/2021/001
[QR] duplicate skipped: CSC/2021/001
[QR] decoded: CSC/2021/002
[HTTP] POST 200 — reg: CSC/2021/002

[SYS] scanner IDLE
```

---

## Troubleshooting

**Camera init failed (0x105):** Usually a loose ribbon cable between the camera module and the ESP32-CAM board. Reseat it firmly — it is the most common hardware issue with this board.

**WiFi keeps disconnecting during upload:** The ESP32-CAM draws high current during flash. Power it from a separate USB supply (phone charger / power bank) via the 5V pin rather than solely from the USB-TTL adapter.

**QR decode fails / out of memory:** The `ESP32QRCodeReader` library uses PSRAM automatically. If you still see memory errors, ensure the board is genuinely an AI-Thinker ESP32-CAM (it has 4MB PSRAM). Generic ESP32 boards without PSRAM will not work with this library.

**Upload fails / port not found:** Confirm IO0 is grounded before powering on. If the board was already powered without IO0 grounded it booted into run mode, not bootloader mode — power-cycle with IO0 grounded.

**Board won't boot after adding button:** GPIO 12 read HIGH at boot due to a missing or incorrect pull-down resistor. Ensure a 10kΩ resistor connects GPIO 12 to GND, and that the button is not pressed during power-on.

**LED stays on after disarming:** Unlikely given the code but if it happens, check for a short between GPIO 13 and 3.3V. The 330Ω series resistor is required — omitting it can damage the GPIO pin.

**Duplicate scans logged:** Expected — the same QR code held in view is deduplicated in firmware. Only a new code (or the first scan of a new armed session) posts to the server.
