#include <WiFi.h>
#include <WebServer.h>
#include "wifi_config.h"

/*
  ESP32 radar real analog microphone firmware.

  Подключение каждого аналогового микрофонного модуля:
    VCC -> 3.3V
    GND -> GND
    OUT -> выбранный вход ESP32

  Нужен именно аналоговый микрофонный модуль с усилителем:
    MAX9814, MAX4466, SparkFun Electret Mic Breakout или похожий.

  Голый электретный микрофон напрямую к ESP32 не подключать.
*/

// =======================================================
// ВЫБОР МИКРОФОНОВ
// =======================================================
// Раскомментируй нужные строки.
// Интерфейс сам покажет все включенные микрофоны.

#define USE_MIC1_D32
#define USE_MIC2_D33
#define USE_MIC3_D34
#define USE_MIC4_D35
#define USE_MIC5_VN

// =======================================================
// СТАТИЧЕСКИЙ IP ESP32
// =======================================================
IPAddress localIp(192, 168, 4, 77);
IPAddress gatewayIp(192, 168, 4, 1);
IPAddress subnetMask(255, 255, 255, 0);
IPAddress dnsIp(192, 168, 4, 1);

WebServer server(80);

const int MAX_MICS = 5;

int micPins[MAX_MICS];
const char* micNames[MAX_MICS];
int micCount = 0;

int rawMinArr[MAX_MICS];
int rawMaxArr[MAX_MICS];
int centerArr[MAX_MICS];
int amplitudeArr[MAX_MICS];
int levelArr[MAX_MICS];
unsigned long samplesArr[MAX_MICS];

unsigned long frameCounter = 0;
unsigned long lastMeasureMs = 0;

const unsigned long SAMPLE_WINDOW_US = 10000;  // 10 ms на каждый активный микрофон
const int SAMPLE_DELAY_US = 80;                // около 12.5 kHz на один канал

const char page[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 real microphones</title>
<body style="margin:0;padding:14px;background:#101622;color:#eef;font-family:sans-serif">
<h2>ESP32 Radar · настоящие аналоговые микрофоны</h2>
<p>Показываются все микрофоны, включённые в прошивке через <b>USE_MIC...</b>.</p>

<canvas id="c" width="380" height="380" style="background:#05070d;border:1px solid #555;width:100%;max-width:520px"></canvas>

<h3>Активные микрофоны</h3>
<div id="mics">waiting...</div>

<h3>Координаты</h3>
<div id="xy">X=0 Y=0</div>

<h3>JSON</h3>
<pre id="p">waiting...</pre>

<script>
const c=document.getElementById("c");
const ctx=c.getContext("2d");
const p=document.getElementById("p");
const mics=document.getElementById("mics");
const xy=document.getElementById("xy");

function clamp(v,a,b){return Math.max(a,Math.min(b,v));}
function map(v,a,b,c,d){v=clamp(v,a,b);return c+(v-a)*(d-c)/(b-a);}

function bar(label,value,maxValue){
  const w=map(value,0,maxValue,0,100);
  return `
    <div style="margin-top:6px">${label}: ${value}</div>
    <div style="height:9px;background:#333;border-radius:10px;overflow:hidden">
      <div style="height:9px;width:${w}%;background:#ffcf4a"></div>
    </div>
  `;
}

function drawRadar(d){
  ctx.fillStyle="#05070d";
  ctx.fillRect(0,0,380,380);

  ctx.strokeStyle="#334";
  ctx.beginPath();
  ctx.moveTo(190,0);
  ctx.lineTo(190,380);
  ctx.moveTo(0,190);
  ctx.lineTo(380,190);
  ctx.stroke();

  for(let r=50;r<=170;r+=40){
    ctx.beginPath();
    ctx.arc(190,190,r,0,Math.PI*2);
    ctx.stroke();
  }

  let px=190+map(d.x,-100,100,-160,160);
  let py=190-map(d.y,-100,100,-160,160);

  ctx.fillStyle="#ffcf4a";
  ctx.beginPath();
  ctx.arc(px,py,14,0,Math.PI*2);
  ctx.fill();

  ctx.fillStyle="#eef";
  ctx.fillText("X="+d.x+" Y="+d.y,12,24);
  ctx.fillText("mics="+d.count,12,44);
}

function renderMics(d){
  let html="";
  for(let i=0;i<d.count;i++){
    html += `
      <div style="margin:10px 0;padding:12px;background:#182033;border-radius:12px">
        <b>${d.names[i]}</b>
        <div>GPIO: ${d.pins[i]}</div>
        ${bar("Amplitude", d.amplitude[i], 1800)}
        ${bar("Level %", d.level_percent[i], 100)}
        <div style="margin-top:6px">Min: ${d.raw_min[i]}</div>
        <div>Max: ${d.raw_max[i]}</div>
        <div>Center: ${d.center[i]}</div>
        <div>Samples: ${d.samples[i]}</div>
      </div>
    `;
  }

  if(d.count === 0){
    html = "<b>Нет активных микрофонов. Раскомментируй USE_MIC...</b>";
  }

  mics.innerHTML=html;
}

async function loop(){
  try{
    const r=await fetch("/api/latest?t="+Date.now(), {cache:"no-store"});
    const d=await r.json();

    p.textContent=JSON.stringify(d,null,2);
    xy.textContent="X="+d.x+" Y="+d.y;
    drawRadar(d);
    renderMics(d);
  }catch(e){
    p.textContent="нет связи: "+e;
  }
  setTimeout(loop,200);
}

loop();
</script>
</body>
)HTML";

void addMic(int pin, const char* name) {
  if (micCount >= MAX_MICS) return;

  micPins[micCount] = pin;
  micNames[micCount] = name;

  rawMinArr[micCount] = 4095;
  rawMaxArr[micCount] = 0;
  centerArr[micCount] = 0;
  amplitudeArr[micCount] = 0;
  levelArr[micCount] = 0;
  samplesArr[micCount] = 0;

  micCount++;
}

void setupMicList() {
#ifdef USE_MIC1_D32
  addMic(32, "MIC1-D32-GPIO32");
#endif

#ifdef USE_MIC2_D33
  addMic(33, "MIC2-D33-GPIO33");
#endif

#ifdef USE_MIC3_D34
  addMic(34, "MIC3-D34-GPIO34");
#endif

#ifdef USE_MIC4_D35
  addMic(35, "MIC4-D35-GPIO35");
#endif

#ifdef USE_MIC5_VN
  addMic(39, "MIC5-VN-GPIO39");
#endif
}

int levelFromAmplitude(int amp) {
  int level = map(amp, 0, 1800, 0, 100);
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  return level;
}

void measureOneMic(int index) {
  int pin = micPins[index];

  int minVal = 4095;
  int maxVal = 0;
  unsigned long count = 0;
  unsigned long startUs = micros();

  while ((micros() - startUs) < SAMPLE_WINDOW_US) {
    int v = analogRead(pin);

    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;

    count++;
    delayMicroseconds(SAMPLE_DELAY_US);
  }

  rawMinArr[index] = minVal;
  rawMaxArr[index] = maxVal;
  centerArr[index] = (minVal + maxVal) / 2;
  amplitudeArr[index] = maxVal - minVal;
  levelArr[index] = levelFromAmplitude(amplitudeArr[index]);
  samplesArr[index] = count;
}

void measureAllMics() {
  for (int i = 0; i < micCount; i++) {
    measureOneMic(i);
  }

  frameCounter++;
}

int findMicByPin(int pin) {
  for (int i = 0; i < micCount; i++) {
    if (micPins[i] == pin) return i;
  }
  return -1;
}

int getAmpByPin(int pin) {
  int index = findMicByPin(pin);
  if (index < 0) return 0;
  return amplitudeArr[index];
}

int normCoord(long value) {
  if (value < -1800) value = -1800;
  if (value > 1800) value = 1800;
  return (int)(value * 100L / 1800L);
}

int calcX() {
  // Левая сторона: D32 + D34
  // Правая сторона: D33 + D35
  long left = ((long)getAmpByPin(32) + (long)getAmpByPin(34)) / 2L;
  long right = ((long)getAmpByPin(33) + (long)getAmpByPin(35)) / 2L;

  return normCoord(right - left);
}

int calcY() {
  // Перед/центр: VN
  // Зад/база: D34 + D35
  long front = (long)getAmpByPin(39);
  long rear = ((long)getAmpByPin(34) + (long)getAmpByPin(35)) / 2L;

  return normCoord(front - rear);
}

void sendJsonArrayInts(const char* name, int* arr) {
  server.sendContent("\"");
  server.sendContent(name);
  server.sendContent("\":[");

  for (int i = 0; i < micCount; i++) {
    if (i) server.sendContent(",");
    server.sendContent(String(arr[i]));
  }

  server.sendContent("]");
}

void sendJsonArrayULongs(const char* name, unsigned long* arr) {
  server.sendContent("\"");
  server.sendContent(name);
  server.sendContent("\":[");

  for (int i = 0; i < micCount; i++) {
    if (i) server.sendContent(",");
    server.sendContent(String(arr[i]));
  }

  server.sendContent("]");
}

void sendJsonNames() {
  server.sendContent("\"names\":[");
  for (int i = 0; i < micCount; i++) {
    if (i) server.sendContent(",");
    server.sendContent("\"");
    server.sendContent(micNames[i]);
    server.sendContent("\"");
  }
  server.sendContent("]");
}

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", page);
}

void handleLatest() {
  int x = calcX();
  int y = calcY();

  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json; charset=utf-8", "");

  server.sendContent("{");

  server.sendContent("\"mode\":\"esp32_radar_real_mics\",");
  server.sendContent("\"count\":");
  server.sendContent(String(micCount));
  server.sendContent(",");

  sendJsonNames();
  server.sendContent(",");

  sendJsonArrayInts("pins", micPins);
  server.sendContent(",");

  sendJsonArrayInts("raw_min", rawMinArr);
  server.sendContent(",");

  sendJsonArrayInts("raw_max", rawMaxArr);
  server.sendContent(",");

  sendJsonArrayInts("center", centerArr);
  server.sendContent(",");

  sendJsonArrayInts("amplitude", amplitudeArr);
  server.sendContent(",");

  sendJsonArrayInts("level_percent", levelArr);
  server.sendContent(",");

  sendJsonArrayULongs("samples", samplesArr);
  server.sendContent(",");

  server.sendContent("\"x\":");
  server.sendContent(String(x));
  server.sendContent(",");

  server.sendContent("\"y\":");
  server.sendContent(String(y));
  server.sendContent(",");

  server.sendContent("\"frame\":");
  server.sendContent(String(frameCounter));
  server.sendContent(",");

  server.sendContent("\"ip\":\"");
  server.sendContent(WiFi.localIP().toString());
  server.sendContent("\",");

  server.sendContent("\"status\":\"online\"");
  server.sendContent("}");
}

void handleNotFound() {
  server.send(404, "text/plain; charset=utf-8", "404");
}

void setup() {
  Serial.begin(115200);
  delay(300);

  analogReadResolution(12);

  setupMicList();

  for (int i = 0; i < micCount; i++) {
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
  Serial.println("===== ESP32 RADAR REAL MICS =====");
  Serial.print("Active mic count: ");
  Serial.println(micCount);

  for (int i = 0; i < micCount; i++) {
    Serial.print("MIC ");
    Serial.print(i);
    Serial.print(": ");
    Serial.print(micNames[i]);
    Serial.print(" GPIO");
    Serial.println(micPins[i]);
  }

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
  Serial.println("  http://192.168.4.77/");
  Serial.println("  http://192.168.4.77/mobile");
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
    measureAllMics();

    Serial.print("DATA:{\"count\":");
    Serial.print(micCount);
    Serial.print(",\"amp\":[");

    for (int i = 0; i < micCount; i++) {
      if (i) Serial.print(",");
      Serial.print(amplitudeArr[i]);
    }

    Serial.print("],\"level\":[");

    for (int i = 0; i < micCount; i++) {
      if (i) Serial.print(",");
      Serial.print(levelArr[i]);
    }

    Serial.print("],\"x\":");
    Serial.print(calcX());
    Serial.print(",\"y\":");
    Serial.print(calcY());
    Serial.print(",\"ip\":\"");
    Serial.print(WiFi.localIP());
    Serial.println("\"}");
  }
}
