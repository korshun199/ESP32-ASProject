#include <WiFi.h>
#include <WebServer.h>
#include "wifi_config.h"

/*
  ESP32 real analog microphone test firmware.

  Подключение микрофонного модуля:
    VCC -> 3.3V
    GND -> GND
    OUT -> выбранный ACTIVE_MIC_PIN

  ВАЖНО:
    Раскомментируй только ОДНУ пару ACTIVE_MIC_PIN / ACTIVE_MIC_NAME.
    Остальные оставь закомментированными.
*/

// ===== ВЫБОР ПОРТА МИКРОФОНА =====

//#define ACTIVE_MIC_PIN 32
//#define ACTIVE_MIC_NAME "MIC1-D32-GPIO32"

//#define ACTIVE_MIC_PIN 33
//#define ACTIVE_MIC_NAME "MIC2-D33-GPIO33"

//#define ACTIVE_MIC_PIN 34
//#define ACTIVE_MIC_NAME "MIC3-D34-GPIO34"

#define ACTIVE_MIC_PIN 35
#define ACTIVE_MIC_NAME "MIC4-D35-GPIO35"

//#define ACTIVE_MIC_PIN 39
//#define ACTIVE_MIC_NAME "MIC5-VN-GPIO39"

#ifndef ACTIVE_MIC_PIN
#error "Раскомментируй один ACTIVE_MIC_PIN"
#endif

#ifndef ACTIVE_MIC_NAME
#error "Раскомментируй один ACTIVE_MIC_NAME"
#endif

// ===== СТАТИЧЕСКИЙ АДРЕС ESP32 В ТВОЕЙ СЕТИ =====
IPAddress localIp(192, 168, 20, 77);
IPAddress gatewayIp(192, 168, 20, 1);
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dnsIp(192, 168, 20, 1);

WebServer server(80);

const unsigned long SAMPLE_WINDOW_US = 20000;   // 20 ms
const int SAMPLE_DELAY_US = 80;                 // примерно 12.5 kHz на одном канале

int rawMin = 4095;
int rawMax = 0;
int rawCenter = 0;
int amplitude = 0;
int levelPercent = 0;
unsigned long samplesCount = 0;
unsigned long frameCounter = 0;
unsigned long lastMeasureMs = 0;

const char page[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 real mic</title>
<body style="margin:0;padding:14px;background:#101622;color:#eef;font-family:sans-serif">
<h2>ESP32 · настоящий аналоговый микрофон</h2>
<p>Режим одного активного микрофона. Порт выбирается в коде через ACTIVE_MIC_PIN.</p>

<canvas id="c" width="360" height="180" style="background:#05070d;border:1px solid #555;width:100%;max-width:520px"></canvas>

<h3 id="name">MIC</h3>
<div id="info">waiting...</div>

<h3>JSON</h3>
<pre id="p">waiting...</pre>

<script>
const c=document.getElementById("c");
const ctx=c.getContext("2d");
const p=document.getElementById("p");
const info=document.getElementById("info");
const nameBox=document.getElementById("name");

function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function map(v,a,b,c,d){v=clamp(v,a,b);return c+(v-a)*(d-c)/(b-a);}

function draw(d){
  ctx.fillStyle="#05070d";
  ctx.fillRect(0,0,360,180);

  ctx.strokeStyle="#334";
  ctx.beginPath();
  ctx.moveTo(0,90);
  ctx.lineTo(360,90);
  ctx.stroke();

  let w=map(d.level_percent,0,100,0,340);
  ctx.fillStyle="#ffcf4a";
  ctx.fillRect(10,70,w,40);

  ctx.fillStyle="#eef";
  ctx.fillText("amplitude: "+d.amplitude,10,25);
  ctx.fillText("level: "+d.level_percent+"%",10,45);
  ctx.fillText("raw min/max: "+d.raw_min+" / "+d.raw_max,10,135);
  ctx.fillText("center: "+d.center,10,155);
}

async function loop(){
  try{
    const r=await fetch("/api/latest?t="+Date.now(), {cache:"no-store"});
    const d=await r.json();

    nameBox.textContent=d.mic_name + " / GPIO" + d.pin;
    info.innerHTML =
      "<b>Amplitude:</b> "+d.amplitude+
      "<br><b>Level:</b> "+d.level_percent+"%"+
      "<br><b>Min:</b> "+d.raw_min+
      "<br><b>Max:</b> "+d.raw_max+
      "<br><b>Center:</b> "+d.center+
      "<br><b>Samples:</b> "+d.samples;

    p.textContent=JSON.stringify(d,null,2);
    draw(d);
  }catch(e){
    p.textContent="нет связи: "+e;
  }
  setTimeout(loop,100);
}
loop();
</script>
</body>
)HTML";

void measureMicrophone() {
  int minVal = 4095;
  int maxVal = 0;
  unsigned long count = 0;

  unsigned long startUs = micros();

  while ((micros() - startUs) < SAMPLE_WINDOW_US) {
    int v = analogRead(ACTIVE_MIC_PIN);

    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;

    count++;
    delayMicroseconds(SAMPLE_DELAY_US);
  }

  rawMin = minVal;
  rawMax = maxVal;
  amplitude = rawMax - rawMin;
  rawCenter = (rawMax + rawMin) / 2;
  samplesCount = count;

  levelPercent = map(amplitude, 0, 1800, 0, 100);
  if (levelPercent < 0) levelPercent = 0;
  if (levelPercent > 100) levelPercent = 100;

  frameCounter++;
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", page);
}

void handleLatest() {
  String json;
  json.reserve(600);

  json += "{";
  json += "\"mode\":\"real_mic_selectable_sta\",";
  json += "\"mic_name\":\"";
  json += ACTIVE_MIC_NAME;
  json += "\",";
  json += "\"pin\":";
  json += ACTIVE_MIC_PIN;
  json += ",";
  json += "\"raw_min\":";
  json += rawMin;
  json += ",";
  json += "\"raw_max\":";
  json += rawMax;
  json += ",";
  json += "\"center\":";
  json += rawCenter;
  json += ",";
  json += "\"amplitude\":";
  json += amplitude;
  json += ",";
  json += "\"level_percent\":";
  json += levelPercent;
  json += ",";
  json += "\"samples\":";
  json += samplesCount;
  json += ",";
  json += "\"frame\":";
  json += frameCounter;
  json += ",";
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
  analogSetPinAttenuation(ACTIVE_MIC_PIN, ADC_11db);

  WiFi.mode(WIFI_STA);

  if (!WiFi.config(localIp, gatewayIp, subnetMask, dnsIp)) {
    Serial.println("Static IP config FAILED");
  } else {
    Serial.print("Static IP configured: ");
    Serial.println(localIp);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.println();
  Serial.println("===== ESP32 REAL MIC SELECTABLE STA =====");
  Serial.print("Active mic: ");
  Serial.println(ACTIVE_MIC_NAME);
  Serial.print("GPIO: ");
  Serial.println(ACTIVE_MIC_PIN);
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
  Serial.println("Open:");
  Serial.println("  http://192.168.20.77/");
  Serial.println("  http://192.168.20.77/mobile");
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

  if (millis() - lastMeasureMs >= 50) {
    lastMeasureMs = millis();
    measureMicrophone();

    Serial.print("DATA:{\"mic\":\"");
    Serial.print(ACTIVE_MIC_NAME);
    Serial.print("\",\"pin\":");
    Serial.print(ACTIVE_MIC_PIN);
    Serial.print(",\"min\":");
    Serial.print(rawMin);
    Serial.print(",\"max\":");
    Serial.print(rawMax);
    Serial.print(",\"amp\":");
    Serial.print(amplitude);
    Serial.print(",\"level\":");
    Serial.print(levelPercent);
    Serial.print(",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  }
}
