import express, { Request, Response } from 'express';

const app  = express();
const PORT = process.env.PORT ? parseInt(process.env.PORT) : 3000;

const RAILWAY_URL = 'https://attendanceapi-production-8964.up.railway.app/api/attendance/scan';

app.use(express.json());

app.post('/scan', async (req: Request, res: Response) => {
  console.log('[PROXY] received scan from ESP32, forwarding to Railway...');

  try {
    const railwayRes = await fetch(RAILWAY_URL, {
      method:  'POST',
      headers: { 'Content-Type': 'application/json' },
      body:    JSON.stringify(req.body),
    });

    const data = await railwayRes.json();

    console.log(`[PROXY] Railway responded: ${railwayRes.status}`);
    console.log('[PROXY]', JSON.stringify(data, null, 2));

    res.status(railwayRes.status).json(data);

  } catch (err) {
    console.error('[PROXY] failed to reach Railway:', err);
    res.status(502).json({
      success: false,
      message: 'Proxy could not reach backend server',
    });
  }
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`[PROXY] AttendScan proxy running on port ${PORT}`);
  console.log(`[PROXY] forwarding POST /scan → ${RAILWAY_URL}`);
});
