# AttendScan

QR-based student attendance system built on an ESP32-CAM. The board scans a student's QR code, extracts the registration number, and POSTs it to a backend server. A built-in web monitor page shows live scan status and results on any browser connected to the same network — no LCD required.

---

## Repository Structure

```
attendscan/
├── attendscan-firmware/   # ESP32-CAM Arduino firmware (C++)
└── attendscan-backend/    # Proxy server — forwards scans to production API (Express/TypeScript)
```

---

## How It Works

1. Operator flips the toggle switch to arm the scanner (LED turns solid on)
2. Student presents their QR code to the camera
3. Firmware decodes the QR, extracts the registration number, and POSTs to the proxy
4. Proxy forwards the request to the production backend on Railway
5. LED blinks to confirm result — 3 slow blinks for success, 4 fast for failure
6. Web monitor at `http://<board-ip>` shows student name, reg number, and server response in real time

---

## Hardware

- AI-Thinker ESP32-CAM
- 6-pin toggle switch on GPIO14
- LED + 220Ω resistor on GPIO2
- USB-to-TTL adapter (CH340 or CP2102) for flashing

---

## Quick Start

See the README in each subdirectory for full setup, wiring, and deploy instructions:

- [`attendscan-firmware/README.md`](./attendscan-firmware/README.md) — arduino-cli setup, compiling, uploading, and monitoring
- [`attendscan-backend/README.md`](./attendscan-backend/README.md) — local dev and Render deploy guide
