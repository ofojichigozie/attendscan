#pragma once

#include <Arduino.h>
#include <WebServer.h>

// ---------------------------------------------------------------------------
// Scan status — fixed-size buffers only, no heap String allocation
// ---------------------------------------------------------------------------
struct ScanStatus {
  char state[12];               // "IDLE" | "READY" | "SCANNING"
  char studentName[64];         // from attendance.fullname on success
  char regNumber[32];           // from attendance.reg_number on success
  char lastScanResponseCode[8]; // "201" | "400" | "-1" | etc.
  char lastActivityMessage[128];// firmware/activity event log (arm, disarm, dup, etc.)
  char lastScanMessage[128];    // message tied ONLY to the last actual scan attempt
  char lastScanUptime[24];      // board uptime string at time of last scan
  bool lastScanSuccess;         // true if success:true in last scan response
  bool hasScanned;              // true once at least one scan attempt has completed
};

extern ScanStatus scanStatus;
extern WebServer  webServer;

// ---------------------------------------------------------------------------
// HTML page — stored in flash (PROGMEM) to save RAM
// ---------------------------------------------------------------------------
static const char PAGE_HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AttendScan</title>
<style>
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:#0f1117;color:#e2e8f0;font-family:system-ui,sans-serif;padding:16px;max-width:480px;margin:auto}
  h1{font-size:1.1rem;font-weight:600;color:#a78bfa;margin-bottom:16px;letter-spacing:.05em}
  .card{background:#1e2130;border:1px solid #2d3148;border-radius:10px;padding:14px;margin-bottom:12px}
  .label{font-size:.65rem;font-weight:600;letter-spacing:.08em;text-transform:uppercase;color:#64748b;margin-bottom:6px}
  .value{font-size:.88rem;color:#e2e8f0;word-break:break-all}
  .value.mono{font-family:monospace;font-size:.78rem}
  .badge{display:inline-block;padding:3px 12px;border-radius:999px;font-size:.72rem;font-weight:600;letter-spacing:.05em}
  .badge.idle    {background:#1e293b;color:#64748b}
  .badge.ready   {background:#14532d;color:#4ade80}
  .badge.scanning{background:#1c3461;color:#60a5fa}
  .ok  {color:#4ade80}
  .fail{color:#f87171}
  .warn{color:#fbbf24}
  .row{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:6px}
  .row.center{justify-content:center;text-align:center}
  .row.center .value{text-align:center}
  .muted{color:#475569}
  .meta{font-size:.7rem;color:#475569}
  .divider{border:none;border-top:1px solid #2d3148;margin:8px 0}
  .name{font-size:1rem;font-weight:600;color:#e2e8f0;margin-bottom:2px}
  .reg{font-size:.78rem;color:#94a3b8;font-family:monospace}
  .log{font-size:.78rem;font-family:monospace;padding:8px 10px;background:#0f1117;border-radius:6px;min-height:32px}
</style>
</head>
<body>
<h1>&#9654; AttendScan Monitor</h1>

<div class="card">
  <div class="row">
    <div>
      <div class="label">Scanner State</div>
      <span id="badge" class="badge idle">IDLE</span>
    </div>
    <div class="meta" id="scantime"></div>
  </div>
</div>

<div class="card">
  <div class="label">Last Scan Result</div>
  <div id="student-block" style="display:none">
    <div class="name" id="student-name"></div>
    <div class="reg"  id="student-reg"></div>
    <hr class="divider">
  </div>
  <div class="row" id="result-row">
    <span id="resmsg" class="value"></span>
    <span class="meta" id="rescode"></span>
  </div>
</div>

<div class="card">
  <div class="label">Activity Log</div>
  <div class="log" id="logline">Waiting...</div>
</div>

<script>
function setBadge(state){
  const el=document.getElementById("badge");
  el.className="badge "+state.toLowerCase();
  el.textContent=state;
}
function logClass(ok, code){
  if(ok) return "ok";
  if(code==="-1"||code==="0") return "warn"; // connection issue
  return "fail";
}
async function poll(){
  try{
    const r=await fetch("/status");
    const d=await r.json();
    setBadge(d.state);

    // Student block — only show on success
    const block=document.getElementById("student-block");
    if(d.lastScanSuccess && d.studentName && d.studentName!=="—"){
      document.getElementById("student-name").textContent=d.studentName;
      document.getElementById("student-reg").textContent=d.regNumber;
      block.style.display="block";
    } else {
      block.style.display="none";
    }

    // Response code + message — this card reflects ONLY the last actual
    // scan attempt (server round-trip), never generic firmware/activity events.
    const resultRow=document.getElementById("result-row");
    const codeEl=document.getElementById("rescode");
    const msgEl =document.getElementById("resmsg");
    if(d.hasScanned){
      resultRow.classList.remove("center");
      codeEl.textContent = "HTTP "+d.lastScanResponseCode;
      msgEl.textContent  = d.lastScanMessage || "—";
      msgEl.className    = "value "+logClass(d.lastScanSuccess, d.lastScanResponseCode);
    } else {
      resultRow.classList.add("center");
      codeEl.textContent = "";
      msgEl.textContent  = "No scans yet";
      msgEl.className    = "value muted";
    }

    // Activity log — every firmware/system event, separate from scan results
    document.getElementById("logline").textContent = d.lastActivityMessage || "—";
    document.getElementById("logline").className   = "log "+logClass(d.lastScanSuccess, d.lastScanResponseCode);

    // Scan time
    document.getElementById("scantime").textContent =
      d.lastScanUptime && d.lastScanUptime!=="—" ? "Last: "+d.lastScanUptime : "";

  }catch(e){
    document.getElementById("logline").textContent="Polling error — board may be busy";
    document.getElementById("logline").className="log warn";
  }
}
poll();
setInterval(poll,3000);
</script>
</body>
</html>
)rawhtml";

// ---------------------------------------------------------------------------
// Route handlers
// ---------------------------------------------------------------------------
void handleRoot() {
  webServer.send_P(200, "text/html", PAGE_HTML);
}

void handleStatus() {
  char json[640];
  snprintf(json, sizeof(json),
    "{"
      "\"state\":\"%s\","
      "\"studentName\":\"%s\","
      "\"regNumber\":\"%s\","
      "\"lastScanResponseCode\":\"%s\","
      "\"lastActivityMessage\":\"%s\","
      "\"lastScanMessage\":\"%s\","
      "\"lastScanSuccess\":%s,"
      "\"hasScanned\":%s,"
      "\"lastScanUptime\":\"%s\""
    "}",
    scanStatus.state,
    scanStatus.studentName,
    scanStatus.regNumber,
    scanStatus.lastScanResponseCode,
    scanStatus.lastActivityMessage,
    scanStatus.lastScanMessage,
    scanStatus.lastScanSuccess ? "true" : "false",
    scanStatus.hasScanned ? "true" : "false",
    scanStatus.lastScanUptime
  );
  webServer.send(200, "application/json", json);
}

void handleNotFound() {
  webServer.send(404, "text/plain", "Not found");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void initWebServer() {
  webServer.on("/",       handleRoot);
  webServer.on("/status", handleStatus);
  webServer.onNotFound(handleNotFound);
  webServer.begin();
  Serial.println("[WEB] server started on port 80");
}

void handleWebServer() {
  webServer.handleClient();
}
