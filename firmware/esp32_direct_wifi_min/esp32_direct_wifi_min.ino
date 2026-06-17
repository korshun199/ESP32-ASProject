#include <WiFi.h>
#include <WebServer.h>

#define MIC1 34
#define MIC2 35
#define MIC3 36

const char* SSID = "ESP32-RADAR";
const char* PASS = "12345678";

WebServer server(80);

int vol1 = 0;
int vol2 = 0;
int vol3 = 0;
int f1 = 0;
int f2 = 0;
int f3 = 0;
int x = 0;
int y = 0;
unsigned long frame = 0;

const char page[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 радар</title>
<body style="margin:0;padding:14px;background:#101622;color:#eef;font-family:sans-serif">
<h2>ESP32 · точка звука</h2>
<p>Wi-Fi напрямую с платы, без ноутбука-сервера.</p>
<canvas id="c" width="320" height="320" style="background:#05070d;border:1px solid #555;width:100%;max-width:420px"></canvas>
<pre id="p">waiting...</pre>
<script>
const c=document.getElementById("c");
const ctx=c.getContext("2d");
const p=document.getElementById("p");

function m(v,a,b,c,d){
  v=Math.max(a,Math.min(b,v));
  return c+(v-a)*(d-c)/(b-a);
}

function draw(d){
  ctx.fillStyle="#05070d";
  ctx.fillRect(0,0,320,320);

  ctx.strokeStyle="#334";
  ctx.beginPath();
  ctx.moveTo(160,0);
  ctx.lineTo(160,320);
  ctx.moveTo(0,160);
  ctx.lineTo(320,160);
  ctx.stroke();

  let px=160+m(d.x,-100,100,-130,130);
  let py=160-m(d.y,-100,100,-130,130);

  ctx.fillStyle="#ffcf4a";
  ctx.beginPath();
  ctx.arc(px,py,12,0,7);
  ctx.fill();

  ctx.fillStyle="#eef";
  ctx.fillText("X="+d.x+" Y="+d.y,10,20);
}

async function loop(){
  try{
    let r=await fetch("/api/latest?t="+Date.now());
    let d=await r.json();
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

int norm(long v) {
  if (v < -4095) v = -4095;
  if (v > 4095) v = 4095;
  return (int)(v * 100L / 4095L);
}

void readSensors() {
  vol1 = analogRead(MIC1);
  vol2 = analogRead(MIC2);
  vol3 = analogRead(MIC3);

  f1 = map(vol1, 0, 4095, 100, 800);
  f2 = map(vol2, 0, 4095, 100, 800);
  f3 = map(vol3, 0, 4095, 100, 800);

  x = norm((long)vol2 - vol1);
  y = norm((long)vol3 - ((long)vol1 + vol2) / 2);
  frame++;
}

void root() {
  server.send_P(200, "text/html; charset=utf-8", page);
}

void api() {
  String s = "{";
  s += "\"freq\":[" + String(f1) + "," + String(f2) + "," + String(f3) + "],";
  s += "\"vol\":[" + String(vol1) + "," + String(vol2) + "," + String(vol3) + "],";
  s += "\"x\":" + String(x) + ",";
  s += "\"y\":" + String(y) + ",";
  s += "\"frame\":" + String(frame) + ",";
  s += "\"mode\":\"esp32_ap\",";
  s += "\"ssid\":\"ESP32-RADAR\",";
  s += "\"ip\":\"" + WiFi.softAPIP().toString() + "\",";
  s += "\"status\":\"online\"}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=utf-8", s);
}

void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(MIC1, ADC_11db);
  analogSetPinAttenuation(MIC2, ADC_11db);
  analogSetPinAttenuation(MIC3, ADC_11db);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASS);

  server.on("/", root);
  server.on("/mobile", root);
  server.on("/api/latest", api);
  server.begin();

  Serial.println("ESP32 DIRECT WIFI");
  Serial.println("SSID: ESP32-RADAR");
  Serial.println("PASS: 12345678");
  Serial.print("URL : http://");
  Serial.println(WiFi.softAPIP());
}

void loop() {
  server.handleClient();

  static unsigned long last = 0;

  if (millis() - last > 50) {
    last = millis();
    readSensors();

    Serial.printf(
      "DATA:{\"freq\":[%d,%d,%d],\"vol\":[%d,%d,%d],\"x\":%d,\"y\":%d}\n",
      f1, f2, f3, vol1, vol2, vol3, x, y
    );
  }
}
