#pragma once

// ---------------------------------------------------------------------------
// Environment — set to 1 for production (HTTPS), 0 for local testing (HTTP)
// ---------------------------------------------------------------------------
#define USE_PRODUCTION 1

// ---------------------------------------------------------------------------
// Server URLs
// ---------------------------------------------------------------------------
#define SERVER_URL_DEV  "http://192.168.0.170:3000/scan"
#define SERVER_URL_PROD "https://attendance-api-k5dw.onrender.com/api/attendance/scan"

#if USE_PRODUCTION
  #define SERVER_URL SERVER_URL_PROD
#else
  #define SERVER_URL SERVER_URL_DEV
#endif

// ---------------------------------------------------------------------------
// WiFi credentials
// ---------------------------------------------------------------------------
#define WIFI_SSID     "Wirespot"
#define WIFI_PASSWORD "W12345678T"

// ---------------------------------------------------------------------------
// Hardcoded attendance metadata (sent with every scan)
// ---------------------------------------------------------------------------
#define ATTENDANCE_SUBJECT    "General Class"
#define ATTENDANCE_LECTURER   "Lecturer"
#define ATTENDANCE_LEVEL      "N/A"
#define ATTENDANCE_DEPARTMENT "N/A"

// ---------------------------------------------------------------------------
// UI pins
// GPIO14: switch (active-high with external 10kΩ pull-down to GND)
// GPIO2:  external LED (active-high, 220Ω series resistor to GND)
// ---------------------------------------------------------------------------
#define PIN_SWITCH 14
#define PIN_LED     2

// ---------------------------------------------------------------------------
// LED blink timing and counts
//   2 blinks — QR decoded, sending to server
//   3 blinks — server confirmed success
//   4 blinks — server returned failure
// ---------------------------------------------------------------------------
#define LED_FAST_BLINK_MS      80
#define LED_SLOW_BLINK_MS     300
#define LED_BLINK_QR_DETECTED   2
#define LED_BLINK_SERVER_OK     3
#define LED_BLINK_SERVER_FAIL   4

// ---------------------------------------------------------------------------
// Switch poll interval (ms)
// ---------------------------------------------------------------------------
#define SWITCH_POLL_MS 100
