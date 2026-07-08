/*
  ESP32 Radar: планшетная диагностика направленного микрофона

  Назначение:
    ESP32 поднимает Wi-Fi AP.
    Android-планшет подключается к ESP32.
    В браузере открывается диагностическая панель.

  Wi-Fi:
    SSID: ESP32-RADAR
    PASS: 12345678
    URL:  http://192.168.4.1

  MIC1:
    GPIO34 / ADC1

  Диагностика:
    - громкость
    - частота звуковой волны
    - ручной порог громкости
    - ручное частотное окно
    - кнопка "Запомнить частоту цели"
    - Android-звук в режиме гейгера

  Важно:
    GPIO-пищалки в этой версии нет.
    Звук воспроизводится только планшетом.
*/

#include <WiFi.h>
#include <WebServer.h>

const char* AP_SSID = "ESP32-RADAR";
const char* AP_PASS = "12345678";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_MASK(255, 255, 255, 0);

WebServer server(80);

const int MIC1_PIN = 34;
const float MIC_P2P_FULL_SCALE = 900.0;

unsigned long seq = 0;

struct MicMeasure {
  int raw;
  int rawMin;
  int rawMax;
  int center;
  int peakToPeak;
  float volume;
  unsigned long samples;
  unsigned int crossings;
  unsigned int freq;
};

float clampFloat(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

MicMeasure measureMic1() {
  MicMeasure m;

  m.raw = 0;
  m.rawMin = 4095;
  m.rawMax = 0;
  m.center = 0;
  m.peakToPeak = 0;
  m.volume = 0.0;
  m.samples = 0;
  m.crossings = 0;
  m.freq = 0;

  const unsigned long windowUs = 50000UL;
  const unsigned int sampleDelayUs = 80;
  const int deadband = 8;

  unsigned long startUs = micros();
  unsigned long sum = 0;

  int firstValue = analogRead(MIC1_PIN);
  int baseline = firstValue;
  int state = 0;

  while ((micros() - startUs) < windowUs) {
    int v = analogRead(MIC1_PIN);

    m.raw = v;

    if (v < m.rawMin) {
      m.rawMin = v;
    }

    if (v > m.rawMax) {
      m.rawMax = v;
    }

    sum += v;
    m.samples++;

    baseline = ((baseline * 31) + v) / 32;

    int diff = v - baseline;
    int newState = state;

    if (diff > deadband) {
      newState = 1;
    } else if (diff < -deadband) {
      newState = -1;
    }

    if (state != 0 && newState != 0 && newState != state) {
      m.crossings++;
    }

    if (newState != 0) {
      state = newState;
    }

    delayMicroseconds(sampleDelayUs);
  }

  if (m.samples > 0) {
    m.center = (int)(sum / m.samples);
  } else {
    m.rawMin = 0;
    m.rawMax = 0;
    m.center = 0;
  }

  m.peakToPeak = m.rawMax - m.rawMin;
  m.volume = clampFloat((float)m.peakToPeak / MIC_P2P_FULL_SCALE, 0.0, 1.0);

  unsigned long windowMs = (micros() - startUs) / 1000UL;

  if (windowMs > 0 && m.peakToPeak > 30 && m.crossings >= 2) {
    m.freq = (unsigned int)((m.crossings * 500UL) / windowMs);
  } else {
    m.freq = 0;
  }

  return m;
}

void sendNoCacheHeaders() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Access-Control-Allow-Origin", "*");
}

void sendLatestJson() {
  MicMeasure m = measureMic1();
  seq++;

  sendNoCacheHeaders();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json; charset=utf-8", "");

  server.sendContent("{");
  server.sendContent("\"seq\":");
  server.sendContent(String(seq));
  server.sendContent(",");
  server.sendContent("\"source\":\"esp32_mic_tablet_ap\",");
  server.sendContent("\"mode\":\"tablet_diagnostics\",");
  server.sendContent("\"wifi\":{\"ssid\":\"");
  server.sendContent(AP_SSID);
  server.sendContent("\",\"ip\":\"192.168.4.1\"},");

  server.sendContent("\"mics\":[");
  server.sendContent("{\"id\":1,\"name\":\"MIC1 ВЕРХ GPIO34\",\"pin\":34,\"present\":true,\"raw\":");
  server.sendContent(String(m.raw));
  server.sendContent(",\"min\":");
  server.sendContent(String(m.rawMin));
  server.sendContent(",\"max\":");
  server.sendContent(String(m.rawMax));
  server.sendContent(",\"center\":");
  server.sendContent(String(m.center));
  server.sendContent(",\"p2p\":");
  server.sendContent(String(m.peakToPeak));
  server.sendContent(",\"volume\":");
  server.sendContent(String(m.volume, 4));
  server.sendContent(",\"samples\":");
  server.sendContent(String(m.samples));
  server.sendContent(",\"crossings\":");
  server.sendContent(String(m.crossings));
  server.sendContent(",\"freq\":");
  server.sendContent(String(m.freq));
  server.sendContent("},");

  server.sendContent("{\"id\":2,\"name\":\"MIC2 ПРАВО\",\"present\":false,\"raw\":0,\"min\":0,\"max\":0,\"center\":0,\"p2p\":0,\"volume\":0.0,\"freq\":0},");
  server.sendContent("{\"id\":3,\"name\":\"MIC3 НИЗ\",\"present\":false,\"raw\":0,\"min\":0,\"max\":0,\"center\":0,\"p2p\":0,\"volume\":0.0,\"freq\":0},");
  server.sendContent("{\"id\":4,\"name\":\"MIC4 ЛЕВО\",\"present\":false,\"raw\":0,\"min\":0,\"max\":0,\"center\":0,\"p2p\":0,\"volume\":0.0,\"freq\":0},");
  server.sendContent("{\"id\":5,\"name\":\"MIC5 ЦЕНТР\",\"present\":false,\"raw\":0,\"min\":0,\"max\":0,\"center\":0,\"p2p\":0,\"volume\":0.0,\"freq\":0}");
  server.sendContent("],");

  server.sendContent("\"audio\":{\"volume\":");
  server.sendContent(String(m.volume, 4));
  server.sendContent(",\"freq\":");
  server.sendContent(String(m.freq));
  server.sendContent("},");

  if (m.volume > 0.03) {
    server.sendContent("\"object\":{\"visible\":true,\"x\":0.5,\"y\":0.12,\"sector\":\"ВЕРХ\"}");
  } else {
    server.sendContent("\"object\":null");
  }

  server.sendContent("}");
}

const char TABLET_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="ru">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
  <title>ESP32 Radar — калибровка двигателя</title>
  <style>
    :root {
      --bg: #101318;
      --panel: #171d26;
      --panel2: #202938;
      --text: #edf2ff;
      --muted: #9aa9bf;
      --green: #39d98a;
      --yellow: #ffd166;
      --red: #ff5c5c;
      --cyan: #4cc9f0;
      --gray: #5f6b7a;
    }

    * { box-sizing: border-box; }

    body {
      margin: 0;
      background: var(--bg);
      color: var(--text);
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
    }

    header {
      padding: 16px;
      background: #0b0f14;
      border-bottom: 1px solid #273244;
      position: sticky;
      top: 0;
      z-index: 10;
    }

    h1 {
      margin: 0;
      font-size: 24px;
      color: var(--cyan);
    }

    .sub {
      margin-top: 6px;
      color: var(--muted);
      font-size: 14px;
    }

    .wrap {
      padding: 14px;
      max-width: 1120px;
      margin: 0 auto;
    }

    .status {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 14px;
    }

    .card, .calibration {
      background: var(--panel);
      border: 1px solid #273244;
      border-radius: 14px;
      padding: 14px;
      box-shadow: 0 10px 30px rgba(0,0,0,.25);
    }

    .calibration { margin-bottom: 14px; }

    .label {
      color: var(--muted);
      font-size: 13px;
    }

    .value {
      font-size: 24px;
      font-weight: 700;
      margin-top: 4px;
    }

    .ok { color: var(--green); }
    .warn { color: var(--yellow); }
    .bad { color: var(--red); }
    .muted { color: var(--muted); }

    .cal-title {
      font-size: 20px;
      font-weight: 800;
      color: var(--cyan);
      margin-bottom: 12px;
    }

    .grid2 {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 12px;
    }

    .slider-box {
      background: #0c1017;
      border: 1px solid #293548;
      border-radius: 12px;
      padding: 12px;
    }

    input[type="range"] {
      width: 100%;
      accent-color: var(--cyan);
    }

    .aim-state {
      font-size: 34px;
      font-weight: 900;
      text-align: center;
      padding: 14px;
      border-radius: 14px;
      background: #0c1017;
      border: 1px solid #293548;
      margin-top: 12px;
    }

    .match-line {
      color: var(--muted);
      font-size: 14px;
      margin-top: 8px;
      line-height: 1.45;
    }

    .mic {
      display: grid;
      grid-template-columns: 90px 1fr 90px 100px 100px;
      gap: 10px;
      align-items: center;
      background: var(--panel);
      border: 1px solid #273244;
      border-radius: 14px;
      padding: 12px;
      margin-bottom: 10px;
    }

    .mic-name {
      font-size: 20px;
      font-weight: 700;
    }

    .mic-small {
      color: var(--muted);
      font-size: 13px;
      margin-top: 3px;
    }

    .badge {
      display: inline-block;
      min-width: 70px;
      text-align: center;
      padding: 8px 10px;
      border-radius: 999px;
      font-weight: 800;
      background: var(--panel2);
    }

    .bar-wrap {
      height: 22px;
      background: #0c1017;
      border: 1px solid #293548;
      border-radius: 999px;
      overflow: hidden;
    }

    .bar {
      height: 100%;
      width: 0%;
      background: var(--green);
      transition: width .12s linear;
    }

    .bar.mid { background: var(--yellow); }
    .bar.high { background: var(--red); }

    .big {
      font-size: 22px;
      font-weight: 800;
    }

    .footer {
      color: var(--muted);
      font-size: 13px;
      margin-top: 18px;
      line-height: 1.5;
    }

    button {
      background: var(--cyan);
      color: #001018;
      border: 0;
      border-radius: 12px;
      padding: 12px 16px;
      font-weight: 800;
      font-size: 16px;
      margin: 4px 6px 4px 0;
    }

    button.secondary { background: #9aa9bf; }
    button.warnbtn { background: #ffd166; }

    @media (max-width: 760px) {
      .status, .grid2 { grid-template-columns: 1fr; }

      .mic { grid-template-columns: 72px 1fr; }

      .mic > div:nth-child(n+3) {
        grid-column: span 2;
      }
    }
  </style>
</head>
<body>
  <header>
    <h1>ESP32 Radar — калибровка двигателя</h1>
    <div class="sub">ESP32-RADAR · http://192.168.4.1 · MIC1 GPIO34</div>
  </header>

  <div class="wrap">
    <div class="status">
      <div class="card">
        <div class="label">Связь</div>
        <div id="linkState" class="value warn">ЖДУ...</div>
      </div>
      <div class="card">
        <div class="label">Громкость</div>
        <div id="audioVolume" class="value">0.00</div>
      </div>
      <div class="card">
        <div class="label">Частота</div>
        <div id="audioFreq" class="value">0 Гц</div>
      </div>
    </div>

    <div class="calibration">
      <div class="cal-title">Ручная калибровка цели</div>

      <div class="grid2">
        <div class="slider-box">
          <label for="thresholdSlider">Порог громкости: <b id="thresholdText">0.08</b></label>
          <input id="thresholdSlider" type="range" min="1" max="60" value="8" step="1" oninput="setThresholdFromSlider(this.value)">
          <div class="match-line">Ниже порога цель считается слишком тихой.</div>
        </div>

        <div class="slider-box">
          <label>Совпадение: <b id="aimPercent">0%</b></label>
          <div class="match-line">
            Условие: громкость выше порога + частота внутри окна.
          </div>
        </div>

        <div class="slider-box">
          <label for="freqMinSlider">Частота от: <b id="freqMinText">100</b> Гц</label>
          <input id="freqMinSlider" type="range" min="20" max="3000" value="100" step="10" oninput="setFreqMinFromSlider(this.value)">
        </div>

        <div class="slider-box">
          <label for="freqMaxSlider">Частота до: <b id="freqMaxText">1200</b> Гц</label>
          <input id="freqMaxSlider" type="range" min="20" max="3000" value="1200" step="10" oninput="setFreqMaxFromSlider(this.value)">
        </div>
      </div>

      <div id="aimState" class="aim-state muted">МИМО</div>

      <div class="match-line" id="calInfo">
        Наведи трубку на двигатель, нажми «Запомнить частоту цели», затем подстрой окно.
      </div>

      <div style="margin-top:12px;">
        <button onclick="learnTarget()">Запомнить частоту цели</button>
        <button onclick="enableSound()">Включить звук</button>
        <button class="warnbtn" onclick="disableSound()">Выключить звук</button>
        <button class="secondary" onclick="loadNow()">Обновить</button>
      </div>

      <div id="soundState" class="footer">
        Звук: выключен. На Android надо один раз нажать «Включить звук».
      </div>
    </div>

    <div id="mics"></div>

    <div class="card">
      <div class="footer">
        Это диагностический режим. Двигатель не даёт одну идеальную частоту, поэтому окно частот лучше делать с запасом.
        Потом эти настройки перенесём в постоянную память ESP32.
      </div>
    </div>
  </div>

<script>
const micNames = {
  1: "ВЕРХ",
  2: "ПРАВО",
  3: "НИЗ",
  4: "ЛЕВО",
  5: "ЦЕНТР"
};

let lastOkMs = 0;

let audioCtx = null;
let soundEnabled = false;
let currentVolume = 0;
let currentFreq = 0;
let lastBeepMs = 0;

let triggerThreshold = 0.08;
let freqMin = 100;
let freqMax = 1200;
let aimActive = false;
let aimReason = "МИМО";

function fmtVolume(v) {
  const n = Number(v || 0);
  return n.toFixed(2);
}

function statusOf(mic) {
  const present = !!mic.present;
  const p2p = Number(mic.p2p || 0);
  const vol = Number(mic.volume || 0);

  if (!present || (p2p === 0 && vol === 0)) {
    return ["НЕТ", "muted"];
  }

  if (vol > 0.02 || p2p > 20) {
    return ["ЖИВ", "ok"];
  }

  return ["ТИХО", "warn"];
}

function barClass(volume) {
  if (volume >= 0.7) return "high";
  if (volume >= 0.25) return "mid";
  return "";
}

function setThresholdFromSlider(value) {
  const n = Number(value || 8);
  triggerThreshold = Math.max(0.01, Math.min(0.60, n / 100));

  const el = document.getElementById("thresholdText");
  if (el) el.textContent = triggerThreshold.toFixed(2);

  updateAimState();
}

function setFreqMinFromSlider(value) {
  freqMin = Number(value || 100);

  if (freqMin > freqMax) {
    freqMax = freqMin;
    const maxSlider = document.getElementById("freqMaxSlider");
    if (maxSlider) maxSlider.value = freqMax;
  }

  updateFreqTexts();
  updateAimState();
}

function setFreqMaxFromSlider(value) {
  freqMax = Number(value || 1200);

  if (freqMax < freqMin) {
    freqMin = freqMax;
    const minSlider = document.getElementById("freqMinSlider");
    if (minSlider) minSlider.value = freqMin;
  }

  updateFreqTexts();
  updateAimState();
}

function updateFreqTexts() {
  const minEl = document.getElementById("freqMinText");
  const maxEl = document.getElementById("freqMaxText");

  if (minEl) minEl.textContent = String(freqMin);
  if (maxEl) maxEl.textContent = String(freqMax);
}

function learnTarget() {
  const f = Number(currentFreq || 0);
  const v = Number(currentVolume || 0);

  if (f <= 0 || v <= 0.01) {
    const info = document.getElementById("calInfo");
    if (info) info.textContent = "Цель не запомнена: частота или громкость слишком мала.";
    return;
  }

  const width = Math.max(80, Math.round(f * 0.30));

  freqMin = Math.max(20, f - width);
  freqMax = Math.min(3000, f + width);

  const threshold = Math.max(0.01, Math.min(0.60, v * 0.60));
  triggerThreshold = threshold;

  const minSlider = document.getElementById("freqMinSlider");
  const maxSlider = document.getElementById("freqMaxSlider");
  const thSlider = document.getElementById("thresholdSlider");

  if (minSlider) minSlider.value = freqMin;
  if (maxSlider) maxSlider.value = freqMax;
  if (thSlider) thSlider.value = Math.round(triggerThreshold * 100);

  updateFreqTexts();
  setThresholdFromSlider(Math.round(triggerThreshold * 100));

  const info = document.getElementById("calInfo");
  if (info) {
    info.textContent = `Запомнено: цель ${f} Гц, окно ${freqMin}-${freqMax} Гц, порог ${triggerThreshold.toFixed(2)}.`;
  }

  updateAimState();
}

function updateAimState() {
  const aim = document.getElementById("aimState");
  const percent = document.getElementById("aimPercent");

  const volumeOk = currentVolume >= triggerThreshold;
  const freqOk = currentFreq >= freqMin && currentFreq <= freqMax && currentFreq > 0;

  const volRatio = triggerThreshold > 0 ? currentVolume / triggerThreshold : 0;
  const freqCenter = (freqMin + freqMax) / 2;
  const freqHalf = Math.max(1, (freqMax - freqMin) / 2);
  const freqError = currentFreq > 0 ? Math.abs(currentFreq - freqCenter) : 99999;
  const freqScore = currentFreq > 0 ? Math.max(0, 1 - (freqError / freqHalf)) : 0;

  let score = 0;

  if (volumeOk && freqOk) {
    score = 100;
  } else {
    score = Math.round(Math.max(0, Math.min(100, (Math.min(volRatio, 1) * 50) + (freqScore * 50))));
  }

  if (percent) percent.textContent = `${score}%`;

  if (!volumeOk) {
    aimActive = false;
    aimReason = "ТИХО";
  } else if (!freqOk) {
    aimActive = false;
    aimReason = "НЕ ТА ЧАСТОТА";
  } else {
    aimActive = true;
    aimReason = "НАВЕДЕНО";
  }

  if (!aim) return;

  aim.textContent = aimReason;

  if (aimActive) {
    aim.className = "aim-state ok";
  } else if (aimReason === "НЕ ТА ЧАСТОТА") {
    aim.className = "aim-state warn";
  } else {
    aim.className = "aim-state muted";
  }
}

function setSoundLabel(text) {
  const el = document.getElementById("soundState");
  if (el) el.textContent = text;
}

function enableSound() {
  try {
    if (!audioCtx) {
      audioCtx = new (window.AudioContext || window.webkitAudioContext)();
    }

    audioCtx.resume();
    soundEnabled = true;
    lastBeepMs = 0;

    playBeep(0.25);
    setSoundLabel("Звук: включен. Пищит только при статусе НАВЕДЕНО.");
  } catch (e) {
    setSoundLabel("Звук: ошибка запуска Web Audio.");
  }
}

function disableSound() {
  soundEnabled = false;
  setSoundLabel("Звук: выключен.");
}

function volumeToPeriodMs(volume) {
  const minVolume = triggerThreshold;

  if (volume < minVolume) return 0;

  const v = Math.max(minVolume, Math.min(1, volume));
  const k = (v - minVolume) / (1 - minVolume);

  return 900 - ((900 - 80) * k);
}

function playBeep(volume) {
  if (!audioCtx || !soundEnabled) return;

  const now = audioCtx.currentTime;
  const v = Math.max(0.02, Math.min(1, volume));

  const osc = audioCtx.createOscillator();
  const gain = audioCtx.createGain();

  osc.type = "square";
  osc.frequency.value = 650 + Math.round(v * 1200);

  const peak = 0.05 + (v * 0.18);

  gain.gain.setValueAtTime(0.0001, now);
  gain.gain.exponentialRampToValueAtTime(peak, now + 0.006);
  gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.055);

  osc.connect(gain);
  gain.connect(audioCtx.destination);

  osc.start(now);
  osc.stop(now + 0.065);
}

function updateAndroidGeiger() {
  if (!soundEnabled || !audioCtx) return;
  if (!aimActive) return;

  const period = volumeToPeriodMs(currentVolume);

  if (period <= 0) return;

  const nowMs = Date.now();

  if ((nowMs - lastBeepMs) >= period) {
    lastBeepMs = nowMs;
    playBeep(currentVolume);
  }
}

function render(data) {
  lastOkMs = Date.now();

  document.getElementById("linkState").textContent = "ЕСТЬ";
  document.getElementById("linkState").className = "value ok";

  const audio = data.audio || {};
  const av = Number(audio.volume || 0);
  const af = Number(audio.freq || 0);

  currentVolume = av;
  currentFreq = af;
  updateAimState();

  document.getElementById("audioVolume").textContent = fmtVolume(av);
  document.getElementById("audioFreq").textContent = `${af} Гц`;

  const mics = data.mics || [];
  const byId = {};

  for (const m of mics) {
    byId[Number(m.id)] = m;
  }

  let html = "";

  for (let id = 1; id <= 5; id++) {
    const m = byId[id] || {
      id,
      name: micNames[id] || `MIC${id}`,
      present: false,
      raw: 0,
      min: 0,
      max: 0,
      center: 0,
      p2p: 0,
      volume: 0,
      freq: 0
    };

    const vol = Number(m.volume || 0);
    const width = Math.max(0, Math.min(100, vol * 100));
    const [st, cls] = statusOf(m);

    html += `
      <div class="mic">
        <div>
          <span class="badge ${cls}">MIC${id}</span>
        </div>
        <div>
          <div class="mic-name">${m.name || micNames[id]}</div>
          <div class="mic-small">raw ${m.raw || 0} · min ${m.min || 0} · max ${m.max || 0} · p2p ${m.p2p || 0} · center ${m.center || 0}</div>
        </div>
        <div>
          <div class="label">Статус</div>
          <div class="big ${cls}">${st}</div>
        </div>
        <div>
          <div class="label">Громкость</div>
          <div class="big">${fmtVolume(vol)}</div>
        </div>
        <div>
          <div class="label">Частота</div>
          <div class="big">${Number(m.freq || 0)} Гц</div>
        </div>
        <div style="grid-column: 1 / -1;">
          <div class="bar-wrap">
            <div class="bar ${barClass(vol)}" style="width:${width}%"></div>
          </div>
        </div>
      </div>
    `;
  }

  document.getElementById("mics").innerHTML = html;
}

async function loadNow() {
  try {
    const r = await fetch("/api/latest", {cache: "no-store"});
    if (!r.ok) throw new Error("HTTP " + r.status);
    const data = await r.json();
    render(data);
  } catch (e) {
    document.getElementById("linkState").textContent = "НЕТ";
    document.getElementById("linkState").className = "value bad";
  }
}

setInterval(loadNow, 300);
setInterval(updateAndroidGeiger, 25);

setInterval(() => {
  if (Date.now() - lastOkMs > 1500) {
    document.getElementById("linkState").textContent = "НЕТ";
    document.getElementById("linkState").className = "value bad";
  }
}, 500);

setThresholdFromSlider(document.getElementById("thresholdSlider").value);
setFreqMinFromSlider(document.getElementById("freqMinSlider").value);
setFreqMaxFromSlider(document.getElementById("freqMaxSlider").value);
loadNow();
</script>
</body>
</html>
)HTML";

void handleRoot() {
  sendNoCacheHeaders();
  server.send(200, "text/html; charset=utf-8", TABLET_HTML);
}

void handleNotFound() {
  sendNoCacheHeaders();
  server.send(404, "text/plain; charset=utf-8", "Не найдено");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  analogReadResolution(12);
  analogSetPinAttenuation(MIC1_PIN, ADC_11db);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_MASK);

  bool apOk = WiFi.softAP(AP_SSID, AP_PASS, 6);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/tablet", HTTP_GET, handleRoot);
  server.on("/tablet.html", HTTP_GET, handleRoot);
  server.on("/api/latest", HTTP_GET, sendLatestJson);
  server.onNotFound(handleNotFound);

  server.begin();

  Serial.println();
  Serial.println("# ESP32 Radar tablet diagnostics started");
  Serial.print("# softAP: ");
  Serial.println(apOk ? "OK" : "FAIL");
  Serial.println("# Wi-Fi AP: ESP32-RADAR / 12345678");
  Serial.print("# AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("# AP MAC: ");
  Serial.println(WiFi.softAPmacAddress());
  Serial.println("# URL: http://192.168.4.1");
}

void loop() {
  server.handleClient();
}
