# attendscan-backend

Proxy server for the AttendScan project. The ESP32-CAM cannot make HTTPS requests directly to Railway (TLS 1.3 incompatibility with the ESP32's mbedTLS stack). This server sits in between — the ESP32 POSTs to this proxy over plain HTTP, and the proxy forwards the request to Railway over HTTPS.

```
ESP32-CAM  →  HTTP  →  this proxy (Render)  →  HTTPS  →  Railway backend
```

---

## Requirements

- Node.js 18 or later (native fetch is required — no extra dependencies)
- npm

---

## Cloning

```bash
git clone https://github.com/ofojichigozie/attendscan.git
cd attendscan-backend
```

---

## Local development

```bash
npm install
npm run dev
```

The proxy listens on port 3000. Point the firmware's `SERVER_URL_DEV` at
`http://<your-lan-ip>:3000/scan` for local testing. The proxy will forward to Railway even from your local machine, so you can verify the full chain before deploying.

---

## Deploying to Render

1. Push this repo to GitHub.
2. Go to [render.com](https://render.com) → New → Web Service.
3. Connect your GitHub repo.
4. Set the following:

| Field | Value |
|---|---|
| Environment | Node |
| Build command | `npm install && npm run build` |
| Start command | `npm start` |

5. Deploy. Render will give you a URL like `https://attendscan-backend.onrender.com`.
6. Update `SERVER_URL_PROD` in the firmware's `config.h` to `https://attendscan-backend.onrender.com/scan` and recompile.

> Render's free tier spins down after inactivity — the first request after idle
> may take 30–50 seconds. This is fine for a prototype. If you want it always
> on, upgrade to a paid instance or use a free uptime monitor like UptimeRobot
> to ping it every 5 minutes.

---

## API

### POST /scan

Accepts the same body the firmware sends and forwards it verbatim to Railway:

```json
{
  "reg_number": "CSC/2024/002",
  "subject": "General Class",
  "lecturer_name": "Lecturer",
  "level": "N/A",
  "department": "N/A"
}
```

Returns Railway's response as-is (status code and body unchanged).

---

## Building

```bash
npm run build   # compiles TypeScript to dist/
npm start       # runs compiled output
```
