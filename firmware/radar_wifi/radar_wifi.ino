#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

const char* AP_SSID = "ESP32-RADAR";
const char* AP_PASS = "12345678";

const int MIC1_PIN = 34;
const int MIC2_PIN = 35;
const int MIC3_PIN = 36;

struct MicData {
  int raw;
  int vol;
  int minv;
  int maxv;
};

MicData readMicWindow(int pin) {
  const int samples = 400;
  int minv = 4095;
  int maxv = 0;
  long sum = 0;

  for (int i = 0; i < samples; i++) {
    int v = analogRead(pin);
    if (v < minv) minv = v;
    if (v > maxv) maxv = v;
    sum += v;
    delayMicroseconds(80);
  }

  MicData d;
  d.raw = sum / samples;
  d.vol = maxv - minv;
  d.minv = minv;
  d.maxv = maxv;
  return d;
}

String makeJson() {
  MicData m1 = readMicWindow(MIC1_PIN);

  int v1 = m1.vol;
  int v2 = 0;
  int v3 = 0;

  int f1 = map(v1, 0, 1200, 100, 1200);
  int f2 = 100;
  int f3 = 100;

  if (f1 < 100) f1 = 100;
  if (f1 > 1200) f1 = 1200;

  int x = map(v2 - v1, -1200, 1200, -100, 100);
  int y = map(v3 - ((v1 + v2) / 2), -1200, 1200, -100, 100);

  String json = "{";
  json += "\"freq\":[" + String(f1) + "," + String(f2) + "," + String(f3) + "],";
  json += "\"vol\":[" + String(v1) + "," + String(v2) + "," + String(v3) + "],";
  json += "\"raw\":[" + String(m1.raw) + ",0,0],";
  json += "\"min\":[" + String(m1.minv) + ",0,0],";
  json += "\"max\":[" + String(m1.maxv) + ",0,0],";
  json += "\"x\":" + String(x) + ",";
  json += "\"y\":" + String(y) + ",";
  json += "\"status\":\"online\"";
  json += "}";
  return json;
}

void handleApiLatest() {
  server.send(200, "application/json", makeJson());
}

void handleRoot() {
  server.send(200, "text/html", R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Radar MAX9814</title>
<style>
body{
  margin:0;
  background:#111;
  color:#eee;
  font-family:Arial,sans-serif;
  text-align:center;
}
h1{
  margin:14px 0 8px 0;
  font-size:24px;
}
.wrap{
  max-width:900px;
  margin:0 auto;
  padding:10px;
}
.panel{
  background:#1b1b1b;
  border:1px solid #444;
  border-radius:14px;
  padding:12px;
  margin:12px auto;
}
canvas{
  width:100%;
  max-width:860px;
  background:#181818;
  border:1px solid #444;
  border-radius:10px;
  display:block;
  margin:10px auto;
}
pre{
  background:#181818;
  border:1px solid #444;
  border-radius:10px;
  text-align:left;
  padding:12px;
  white-space:pre-wrap;
  word-break:break-word;
  overflow:auto;
}
.info{
  display:flex;
  gap:10px;
  justify-content:center;
  flex-wrap:wrap;
  margin-bottom:8px;
}
.badge{
  background:#262626;
  border:1px solid #444;
  border-radius:999px;
  padding:8px 14px;
  font-size:14px;
}
.state{
  font-weight:bold;
}
</style>
</head>
<body>
<div class="wrap">
  <h1>ESP32 Radar / MAX9814</h1>

  <div class="info">
    <div class="badge">Wi-Fi: ESP32-RADAR</div>
    <div class="badge">Mic: GPIO34</div>
    <div class="badge">Mode: Loudness Graph</div>
    <div class="badge state" id="state">СОСТОЯНИЕ: ...</div>
  </div>

  <div class="panel">
    <h2>Текущая громкость</h2>
    <canvas id="levelCanvas" width="860" height="120"></canvas>
  </div>

  <div class="panel">
    <h2>График громкости во времени</h2>
    <canvas id="graphCanvas" width="860" height="260"></canvas>
  </div>

  <div class="panel">
    <h2>Сырые данные</h2>
    <pre id="data">waiting...</pre>
  </div>
</div>

<script>
const levelCanvas = document.getElementById('levelCanvas');
const levelCtx = levelCanvas.getContext('2d');

const graphCanvas = document.getElementById('graphCanvas');
const graphCtx = graphCanvas.getContext('2d');

const out = document.getElementById('data');
const stateEl = document.getElementById('state');

const volHistory = [];
const rawHistory = [];
const MAX_POINTS = 220;

function drawLevel(d) {
  const w = levelCanvas.width;
  const h = levelCanvas.height;
  levelCtx.clearRect(0,0,w,h);

  const vol = d.vol[0] || 0;
  const raw = d.raw[0] || 0;

  let ratio = vol / 300.0;
  if (ratio > 1) ratio = 1;
  if (ratio < 0) ratio = 0;

  let color = '#00cc66';
  let state = 'ТИШИНА';

  if (vol > 40) {
    color = '#d4c400';
    state = 'СЛАБЫЙ ЗВУК';
  }
  if (vol > 90) {
    color = '#ff7a00';
    state = 'РЕЧЬ / ЗВУК';
  }
  if (vol > 180) {
    color = '#ff3333';
    state = 'ГРОМКО';
  }

  stateEl.textContent = 'СОСТОЯНИЕ: ' + state;

  levelCtx.fillStyle = '#222';
  levelCtx.fillRect(20, 40, w - 40, 30);

  levelCtx.fillStyle = color;
  levelCtx.fillRect(20, 40, (w - 40) * ratio, 30);

  levelCtx.strokeStyle = '#777';
  levelCtx.strokeRect(20, 40, w - 40, 30);

  levelCtx.fillStyle = '#fff';
  levelCtx.font = '18px Arial';
  levelCtx.fillText('vol = ' + vol, 20, 28);
  levelCtx.fillText('raw = ' + raw, 180, 28);
  levelCtx.fillText(state, 20, 98);
}

function drawGraph() {
  const w = graphCanvas.width;
  const h = graphCanvas.height;
  graphCtx.clearRect(0,0,w,h);

  graphCtx.fillStyle = '#181818';
  graphCtx.fillRect(0,0,w,h);

  let maxY = 120;
  for (let i = 0; i < volHistory.length; i++) {
    if (volHistory[i] > maxY) maxY = volHistory[i];
  }
  maxY = Math.ceil(maxY * 1.15);
  if (maxY < 120) maxY = 120;

  graphCtx.strokeStyle = '#333';
  graphCtx.lineWidth = 1;

  for (let i = 0; i <= 5; i++) {
    const y = 20 + i * ((h - 40) / 5);
    graphCtx.beginPath();
    graphCtx.moveTo(40, y);
    graphCtx.lineTo(w - 10, y);
    graphCtx.stroke();

    const val = Math.round(maxY - (i * maxY / 5));
    graphCtx.fillStyle = '#aaa';
    graphCtx.font = '12px Arial';
    graphCtx.fillText(val, 5, y + 4);
  }

  graphCtx.strokeStyle = '#555';
  graphCtx.beginPath();
  graphCtx.moveTo(40, 20);
  graphCtx.lineTo(40, h - 20);
  graphCtx.lineTo(w - 10, h - 20);
  graphCtx.stroke();

  graphCtx.strokeStyle = '#0f0';
  graphCtx.lineWidth = 2;
  graphCtx.beginPath();

  for (let i = 0; i < volHistory.length; i++) {
    const x = 40 + i * ((w - 50) / (MAX_POINTS - 1));
    const y = (h - 20) - ((volHistory[i] / maxY) * (h - 40));
    if (i === 0) graphCtx.moveTo(x, y);
    else graphCtx.lineTo(x, y);
  }
  graphCtx.stroke();

  graphCtx.strokeStyle = '#ffaa00';
  graphCtx.lineWidth = 1;
  const thr = 60;
  const yThr = (h - 20) - ((thr / maxY) * (h - 40));
  graphCtx.beginPath();
  graphCtx.moveTo(40, yThr);
  graphCtx.lineTo(w - 10, yThr);
  graphCtx.stroke();

  graphCtx.fillStyle = '#ffaa00';
  graphCtx.font = '12px Arial';
  graphCtx.fillText('Порог речи ~ ' + thr, w - 130, yThr - 6);

  if (volHistory.length > 0) {
    const last = volHistory[volHistory.length - 1];
    graphCtx.fillStyle = '#fff';
    graphCtx.font = '16px Arial';
    graphCtx.fillText('Текущая громкость: ' + last, 50, 18);
  }
}

async function tick() {
  try {
    const r = await fetch('/api/latest');
    const d = await r.json();

    out.textContent = JSON.stringify(d, null, 2);

    const vol = d.vol[0] || 0;
    const raw = d.raw[0] || 0;

    volHistory.push(vol);
    rawHistory.push(raw);

    if (volHistory.length > MAX_POINTS) volHistory.shift();
    if (rawHistory.length > MAX_POINTS) rawHistory.shift();

    drawLevel(d);
    drawGraph();
  } catch (e) {
    out.textContent = 'offline: ' + e;
    stateEl.textContent = 'СОСТОЯНИЕ: OFFLINE';
  }
}

setInterval(tick, 150);
tick();
</script>
</body>
</html>
)HTML");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetPinAttenuation(MIC1_PIN, ADC_11db);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/api/latest", handleApiLatest);
  server.begin();

  Serial.println("ESP32 Radar MAX9814 started");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("PASS: ");
  Serial.println(AP_PASS);
  Serial.print("URL: http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();
}
