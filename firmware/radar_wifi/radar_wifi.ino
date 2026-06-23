#include <WiFi.h>
#include <WebServer.h>

WebServer server(80);

const char* AP_SSID = "ESP32-RADAR";
const char* AP_PASS = "12345678";

const int MIC1_PIN = 34;
const int MIC2_PIN = 35;
const int MIC3_PIN = 36;

String makeJson() {
  int v1 = analogRead(MIC1_PIN);
  int v2 = analogRead(MIC2_PIN);
  int v3 = analogRead(MIC3_PIN);

  int f1 = map(v1, 0, 4095, 100, 1200);
  int f2 = map(v2, 0, 4095, 100, 1200);
  int f3 = map(v3, 0, 4095, 100, 1200);

  int x = map(v2 - v1, -4095, 4095, -100, 100);
  int y = map(v3 - ((v1 + v2) / 2), -4095, 4095, -100, 100);

  String json = "{";
  json += "\"freq\":[" + String(f1) + "," + String(f2) + "," + String(f3) + "],";
  json += "\"vol\":[" + String(v1) + "," + String(v2) + "," + String(v3) + "],";
  json += "\"x\":" + String(x) + ",";
  json += "\"y\":" + String(y) + ",";
  json += "\"status\":\"online\"";
  json += "}";
  return json;
}

void handleRoot() {
  server.send(200, "text/html", R"HTML(
<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>ESP32 Radar</title>
<style>
body{margin:0;background:#111;color:#eee;font-family:Arial;text-align:center}
h1{margin:14px}
#box{display:flex;justify-content:center;gap:20px;flex-wrap:wrap}
canvas{background:#222;border:1px solid #555;border-radius:12px}
pre{background:#222;padding:12px;border-radius:12px;text-align:left;min-width:320px}
</style>
</head>
<body>
<h1>ESP32 Radar Wi-Fi</h1>
<div id="box">
<canvas id="radar" width="360" height="360"></canvas>
<pre id="data">waiting...</pre>
</div>
<script>
const c=document.getElementById('radar');
const ctx=c.getContext('2d');
const out=document.getElementById('data');

function draw(d){
  ctx.clearRect(0,0,c.width,c.height);
  ctx.strokeStyle='#666';
  ctx.beginPath();
  ctx.arc(180,180,150,0,Math.PI*2);
  ctx.moveTo(30,180); ctx.lineTo(330,180);
  ctx.moveTo(180,30); ctx.lineTo(180,330);
  ctx.stroke();

  const px=180+d.x*1.5;
  const py=180-d.y*1.5;

  ctx.fillStyle='#fff';
  ctx.beginPath();
  ctx.arc(180,310,8,0,Math.PI*2); ctx.fill();
  ctx.beginPath();
  ctx.arc(60,60,8,0,Math.PI*2); ctx.fill();
  ctx.beginPath();
  ctx.arc(300,60,8,0,Math.PI*2); ctx.fill();

  ctx.fillStyle='#0f0';
  ctx.beginPath();
  ctx.arc(px,py,12,0,Math.PI*2);
  ctx.fill();
}

async function tick(){
  try{
    const r=await fetch('/api/latest');
    const d=await r.json();
    out.textContent=JSON.stringify(d,null,2);
    draw(d);
  }catch(e){
    out.textContent='offline: '+e;
  }
}
setInterval(tick,300);
tick();
</script>
</body>
</html>
)HTML");
}

void handleApiLatest() {
  server.send(200, "application/json", makeJson());
}

void setup() {
  Serial.begin(115200);
  delay(500);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/api/latest", handleApiLatest);
  server.begin();

  Serial.println("ESP32 Radar Wi-Fi started");
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
