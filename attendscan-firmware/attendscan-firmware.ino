#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <ESP32QRCodeReader.h>
#include "config.h"
#include "attendscan_webserver.h"

// ESP32QRCodeReader targets the AI-Thinker module and manages camera init internally.
// FRAMESIZE_QVGA (320x240) is used — sufficient for a plain reg_number QR code
// and keeps PSRAM usage low enough to run the web server concurrently.
ESP32QRCodeReader reader(CAMERA_MODEL_AI_THINKER, FRAMESIZE_QVGA);
WebServer         webServer(80);

// Scanner states
enum class ScannerState { IDLE, READY };

static ScannerState scannerState = ScannerState::IDLE;

// Global scan status — written by firmware, read by web server
ScanStatus scanStatus = {
  .state                = "IDLE",
  .studentName          = "—",
  .regNumber            = "—",
  .lastScanResponseCode = "—",
  .lastActivityMessage  = "System started",
  .lastScanMessage      = "No scans yet",
  .lastScanUptime       = "—",
  .lastScanSuccess      = false,
  .hasScanned           = false
};

// ---------------------------------------------------------------------------
// LED
// ---------------------------------------------------------------------------
void setLed(bool on) { digitalWrite(PIN_LED, on ? HIGH : LOW); }

void blinkLed(int count, int intervalMs) {
  for (int i = 0; i < count; i++) {
    setLed(true);  delay(intervalMs);
    setLed(false); delay(intervalMs);
  }
}

// ---------------------------------------------------------------------------
// Helper — safely copy a string into a fixed char buffer
// ---------------------------------------------------------------------------
void setStatus(char* buf, size_t bufLen, const char* value) {
  strncpy(buf, value, bufLen - 1);
  buf[bufLen - 1] = '\0';
}

// ---------------------------------------------------------------------------
// Switch — polled every SWITCH_POLL_MS
// Switch DOWN (HIGH) → READY, Switch UP (LOW) → IDLE
// ---------------------------------------------------------------------------
void checkSwitch() {
  bool switchOn = digitalRead(PIN_SWITCH) == HIGH;

  if (switchOn && scannerState == ScannerState::IDLE) {
    scannerState = ScannerState::READY;
    setLed(true);
    setStatus(scanStatus.state,               sizeof(scanStatus.state),               "READY");
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage),  "Scanner armed — waiting for QR code");
    Serial.println("[SYS] scanner READY — waiting for QR codes");

  } else if (!switchOn && scannerState == ScannerState::READY) {
    scannerState = ScannerState::IDLE;
    setLed(false);
    setStatus(scanStatus.state,               sizeof(scanStatus.state),               "IDLE");
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage),  "Scanner disarmed");
    Serial.println("[SYS] scanner IDLE");
  }
}

// ---------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------
void connectToWiFi() {
  Serial.printf("[WiFi] connecting to %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.printf("\n[WiFi] connected — IP: %s\n", WiFi.localIP().toString().c_str());
}

// ---------------------------------------------------------------------------
// JSON helpers
// ---------------------------------------------------------------------------
String escapeJson(const String& raw) {
  String out = "";
  for (int i = 0; i < raw.length(); i++) {
    if      (raw[i] == '"')  out += "\\\"";
    else if (raw[i] == '\\') out += "\\\\";
    else                     out += raw[i];
  }
  return out;
}

String buildPayload(const String& regNumber) {
  String body = "{";
  body += "\"reg_number\":\""    + escapeJson(regNumber)           + "\",";
  body += "\"subject\":\""       + String(ATTENDANCE_SUBJECT)      + "\",";
  body += "\"lecturer_name\":\"" + String(ATTENDANCE_LECTURER)     + "\",";
  body += "\"level\":\""         + String(ATTENDANCE_LEVEL)        + "\",";
  body += "\"department\":\""    + String(ATTENDANCE_DEPARTMENT)   + "\"";
  body += "}";
  return body;
}

String extractJsonString(const String& json, const char* key) {
  String search = String("\"") + key + "\":\"";
  int start = json.indexOf(search);
  if (start == -1) return "";
  start += search.length();
  int end = json.indexOf("\"", start);
  if (end == -1) return "";
  return json.substring(start, end);
}

void formatUptime(char* buf, size_t len) {
  unsigned long s = millis() / 1000;
  unsigned long m = s / 60; s %= 60;
  unsigned long h = m / 60; m %= 60;
  snprintf(buf, len, "%02luh %02lum %02lus", h, m, s);
}

// ---------------------------------------------------------------------------
// Server
// ---------------------------------------------------------------------------
void sendScanData(const String& regNumber) {
  String body         = buildPayload(regNumber);
  int    statusCode   = -1;
  String responseBody = "";

  setStatus(scanStatus.state,               sizeof(scanStatus.state),               "SCANNING");
  setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage),  "Sending to server...");

#if USE_PRODUCTION
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(15);          // 15s socket timeout — Railway may be cold-starting

  HTTPClient http;
  http.begin(client, SERVER_URL);
  http.setTimeout(15000);         // 15s HTTP timeout
  http.addHeader("Content-Type", "application/json");
  statusCode   = http.POST(body);
  responseBody = http.getString();
  http.end();
#else
  HTTPClient http;
  http.begin(SERVER_URL);
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  statusCode   = http.POST(body);
  responseBody = http.getString();
  http.end();
#endif

  snprintf(scanStatus.lastScanResponseCode, sizeof(scanStatus.lastScanResponseCode),
           "%d", statusCode);
  formatUptime(scanStatus.lastScanUptime, sizeof(scanStatus.lastScanUptime));
  scanStatus.hasScanned = true;

  if (statusCode > 0) {
    Serial.printf("[HTTP] POST %d\n", statusCode);
    Serial.printf("[HTTP] response: %s\n", responseBody.c_str());

    bool   success = responseBody.indexOf("\"success\":true") != -1;
    String message = extractJsonString(responseBody, "message");
    if (message.isEmpty()) message = success ? "OK" : "Unknown error";

    scanStatus.lastScanSuccess = success;
    setStatus(scanStatus.lastScanMessage,     sizeof(scanStatus.lastScanMessage),     message.c_str());
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage),  message.c_str());

    if (success) {
      String fullname      = extractJsonString(responseBody, "fullname");
      String respRegNumber = extractJsonString(responseBody, "reg_number");
      setStatus(scanStatus.studentName, sizeof(scanStatus.studentName),
                fullname.isEmpty()      ? "—" : fullname.c_str());
      setStatus(scanStatus.regNumber,   sizeof(scanStatus.regNumber),
                respRegNumber.isEmpty() ? "—" : respRegNumber.c_str());
      blinkLed(LED_BLINK_SERVER_OK, LED_SLOW_BLINK_MS);    // 3 slow blinks
    } else {
      setStatus(scanStatus.studentName, sizeof(scanStatus.studentName), "—");
      setStatus(scanStatus.regNumber,   sizeof(scanStatus.regNumber),   "—");
      blinkLed(LED_BLINK_SERVER_FAIL, LED_FAST_BLINK_MS);  // 4 fast blinks
    }

  } else {
    Serial.printf("[HTTP] POST failed: %d\n", statusCode);
    scanStatus.lastScanSuccess = false;
    setStatus(scanStatus.lastScanMessage,     sizeof(scanStatus.lastScanMessage),     "Connection failed — check server");
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage),  "Connection failed — check server");
    setStatus(scanStatus.studentName,         sizeof(scanStatus.studentName),          "—");
    setStatus(scanStatus.regNumber,           sizeof(scanStatus.regNumber),            "—");
    blinkLed(LED_BLINK_SERVER_FAIL, LED_FAST_BLINK_MS);    // 4 fast blinks
  }

  setStatus(scanStatus.state, sizeof(scanStatus.state), "READY");
  setLed(true);
}

// ---------------------------------------------------------------------------
// Arduino entry points
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[SYS] AttendScan firmware starting...");

#if USE_PRODUCTION
  Serial.println("[SYS] mode: PRODUCTION (HTTPS)");
#else
  Serial.println("[SYS] mode: DEVELOPMENT (HTTP)");
#endif

  pinMode(PIN_SWITCH, INPUT);
  pinMode(PIN_LED, OUTPUT);
  setLed(false);

  reader.setup();
  reader.beginOnCore(1);

  connectToWiFi();
  initWebServer();

  Serial.printf("[WEB] open http://%s in your browser\n", WiFi.localIP().toString().c_str());
  Serial.println("[SYS] idle — flip switch to start scanning");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] connection lost — reconnecting...");
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage), "WiFi lost — reconnecting...");
    connectToWiFi();
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage), "WiFi reconnected");
  }

  handleWebServer();

  static unsigned long lastSwitchCheck = 0;
  if (millis() - lastSwitchCheck >= SWITCH_POLL_MS) {
    lastSwitchCheck = millis();
    checkSwitch();
  }

  if (scannerState == ScannerState::IDLE) return;

  struct QRCodeData qrData;
  if (!reader.receiveQrCode(&qrData, 0)) return;

  if (!qrData.valid) {
    setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage), "Invalid QR code — could not decode");
    Serial.println("[QR] invalid code detected, ignoring");
    return;
  }

  String regNumber = String((const char*)qrData.payload);
  Serial.printf("[QR] decoded reg_number: %s\n", regNumber.c_str());
  setStatus(scanStatus.lastActivityMessage, sizeof(scanStatus.lastActivityMessage), "QR decoded — sending to server...");

  blinkLed(LED_BLINK_QR_DETECTED, LED_FAST_BLINK_MS); // 2 fast blinks
  sendScanData(regNumber);
}
