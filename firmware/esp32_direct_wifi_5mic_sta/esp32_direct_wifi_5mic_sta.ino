#include <WiFi.h>
#include <WebServer.h>
#include "wifi_config.h"

IPAddress localIp(192, 168, 20, 77);
IPAddress gatewayIp(192, 168, 20, 1);
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dnsIp(192, 168, 20, 1);

#define MIC_COUNT 5

#define MIC1_PIN 32
#define MIC2_PIN 33
#define MIC3_PIN 34
#define MIC4_PIN 35
#define MIC5_PIN 39

const int micPins[MIC_COUNT] = {
  MIC1_PIN,
  MIC2_PIN,
  MIC3_PIN,
  MIC4_PIN,
  MIC5_PIN
};

const char* micNames[MIC_COUNT] = {
  "MIC1-D32",
  "MIC2-D33",
  "MIC3-D34",
  "MIC4-D35",
  "MIC5-VN"
};

WebServer server(80);

int micVol[MIC_COUNT] = {0, 0, 0, 0, 0};
int micFreq[MIC_COUNT] = {0, 0, 0, 0, 0};

int pointX = 0;
int pointY = 0;
unsigned long frameCounter = 0;
unsigned long lastSampleMs = 0;

const char page[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 5 микрофонов</title>
<body style="margin:0;padding:14px;background:#101622;color:#eef;font-family:sans-serif">
<h2>ESP32 · 5 микрофонов · Wi-Fi сеть</h2>
<p>Плата подключена к обычной Wi-Fi сети. Ноутбук-сервер не используется.</p>

<canvas id="c" width="340" height="340" style="background:#05070d;border:1px solid #555;width:100%;max-width:460px"></canvas>

<h3>Координаты</h3>
<div id="xy">X=0 Y=0</div>

<h3>Микрофоны</h3>
<div id="mics"></div>

<h3>Кадр JSON</h3>
<pre id="p">waiting...</pre>

<script>
const c=document.getElementById("c");
const ctx=c.getContext("2d");
const p=document.getElementById("p");
const mics=document.getElementById("mics");
const xy=document.getElementById("xy");

function clamp(v,a,b){
  return Math.max(a,Math.min(b,v));
}

function map(v,a,b,c,d){
  v=clamp(v,a,b);
  return c+(v-a)*(d-c)/(b-a);
}

function draw(d){
  ctx.fillStyle="#05070d";
  ctx.fillRect(0,0,340,340);

  ctx.strokeStyle="#334";
  ctx.beginPath();
  ctx.moveTo(170,0);
  ctx.lineTo(170,340);
  ctx.moveTo(0,170);
  ctx.lineTo(340,170);
  ctx.stroke();

  ctx.strokeStyle="#223";
  for(let r=50;r<=150;r+=50){
    ctx.beginPath();
    ctx.arc(170,170,r,0,7);
    ctx.stroke();
  }

  let px=170+map(d.x,-100,100,-140,140);
  let py=170-map(d.y,-100,100,-140,140);

  ctx.fillStyle="#ffcf4a";
  ctx.beginPath();
  ctx.arc(px,py,13,0,7);
  ctx.fill();

  ctx.fillStyle="#eef";
  ctx.fillText("X="+d.x+" Y="+d.y,10,22);
}

function renderMics(d){
  let html="";
  for(let i=0;i<d.vol.length;i++){
    let vol=d.vol[i] ?? 0;
    let freq=d.freq[i] ?? 0;
    let name=(d.names && d.names[i]) ? d.names[i] : ("MIC"+(i+1));
    let vw=map(vol,0,4095,0,100);
    let fw=map(freq,100,800,0,100);

    html += "<div style='margin:10px 0;padding:10px;background:#182033;border-radius:10px'>";
    html += "<b>"+name+"</b>";
    html += "<div>Громкость: "+vol+"</div>";
    html += "<div style='height:8px;background:#333'><div style='height:8px;width:"+vw+"%;background:#ffcf4a'></div></div>";
    html += "<div>Частота: "+freq+" Гц</div>";
    html += "<div style='height:8px;background:#333'><div style='height:8px;width:"+fw+"%;background:#4aa3ff'></div></div>";
    html += "</div>";
  }
  mics.innerHTML=html;
}

async function loop(){
  try{
    let r=await fetch("/api/latest?t="+Date.now(), {cache:"no-store"});
    let d=await r.json();
    p.textContent=JSON.stringify(d,null,2);
    xy.textContent="X="+d.x+" Y="+d.y;
    draw(d);
    renderMics(d);
  }catch(e){
    p.textContent="нет связи: "+e;
  }
  setTimeout(loop,100);
}

loop();
</script>
</body>
)HTML";

int normCoord(long value) {
  if (value < -4095) value = -4095;
  if (value > 4095) value = 4095;
  return (int)(value * 100L / 4095L);
}

void readSensors() {
  for (int i = 0; i < MIC_COUNT; i++) {
    micVol[i] = analogRead(micPins[i]);
    micFreq[i] = map(micVol[i], 0, 4095, 100, 800);
  }

  long leftSide = ((long)micVol[0] + (long)micVol[2]) / 2L;
  long rightSide = ((long)micVol[1] + (long)micVol[3]) / 2L;
  long rearSide = ((long)micVol[2] + (long)micVol[3]) / 2L;
  long frontSide = (long)micVol[4];

  pointX = normCoord(rightSide - leftSide);
  pointY = normCoord(frontSide - rearSide);

  frameCounter++;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", page);
}

void handleLatest() {
  String json;
  json.reserve(700);

  json += "{";

  json += "\"freq\":[";
  for (int i = 0; i < MIC_COUNT; i++) {
    if (i) json += ",";
    json += micFreq[i];
  }
  json += "],";

  json += "\"vol\":[";
  for (int i = 0; i < MIC_COUNT; i++) {
    if (i) json += ",";
    json += micVol[i];
  }
  json += "],";

  json += "\"names\":[";
  for (int i = 0; i < MIC_COUNT; i++) {
    if (i) json += ",";
    json += "\"";
    json += micNames[i];
    json += "\"";
  }
  json += "],";

  json += "\"x\":";
  json += pointX;
  json += ",";

  json += "\"y\":";
  json += pointY;
  json += ",";

  json += "\"frame\":";
  json += frameCounter;
  json += ",";

  json += "\"mode\":\"esp32_sta_5mic\",";
  json += "\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",";
  json += "\"status\":\"online\"";
  json += "}";

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", json);
}

void handleNotFound() {
  server.send(404, "text/plain; charset=utf-8", "404");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  analogReadResolution(12);

  for (int i = 0; i < MIC_COUNT; i++) {
    analogSetPinAttenuation(micPins[i], ADC_11db);
  }

  WiFi.mode(WIFI_STA);

  if (!WiFi.config(localIp, gatewayIp, subnetMask, dnsIp)) {
    Serial.println("Static IP config FAILED");
  } else {
    Serial.print("Static IP configured: ");
    Serial.println(localIp);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println();
  Serial.println("===== ESP32 5MIC WIFI STA =====");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Connecting");

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect FAILED");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/mobile", HTTP_GET, handleRoot);
  server.on("/api/latest", HTTP_GET, handleLatest);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("HTTP server started");
  Serial.println("==============================");
}

void loop() {
  server.handleClient();

  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectMs = 0;
    if (millis() - lastReconnectMs > 5000) {
      lastReconnectMs = millis();
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  if (millis() - lastSampleMs >= 50) {
    lastSampleMs = millis();
    readSensors();

    Serial.print("DATA:{\"vol\":[");
    for (int i = 0; i < MIC_COUNT; i++) {
      if (i) Serial.print(",");
      Serial.print(micVol[i]);
    }
    Serial.print("],\"freq\":[");
    for (int i = 0; i < MIC_COUNT; i++) {
      if (i) Serial.print(",");
      Serial.print(micFreq[i]);
    }
    Serial.print("],\"x\":");
    Serial.print(pointX);
    Serial.print(",\"y\":");
    Serial.print(pointY);
    Serial.print(",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  }
}
