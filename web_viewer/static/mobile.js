const canvas = document.getElementById("radar");
const ctx = canvas.getContext("2d");

const statusEl = document.getElementById("status");
const rawEl = document.getElementById("raw");
const xEl = document.getElementById("xValue");
const yEl = document.getElementById("yValue");

const freqEls = [
  document.getElementById("freq1"),
  document.getElementById("freq2"),
  document.getElementById("freq3"),
];

const volEls = [
  document.getElementById("vol1"),
  document.getElementById("vol2"),
  document.getElementById("vol3"),
];

const freqBars = [
  document.getElementById("freqBar1"),
  document.getElementById("freqBar2"),
  document.getElementById("freqBar3"),
];

const volBars = [
  document.getElementById("volBar1"),
  document.getElementById("volBar2"),
  document.getElementById("volBar3"),
];

let latest = {
  freq: [0, 0, 0],
  vol: [0, 0, 0],
  x: 0,
  y: 0,
  status: "ожидание",
};

function translateStatus(status) {
  if (!status) return "ожидание";

  if (status === "online") return "онлайн";
  if (status === "waiting") return "ожидание";
  if (status === "connected") return "подключено";
  if (status === "reconnect") return "переподключение";

  if (String(status).startsWith("connecting")) return "подключение";
  if (String(status).startsWith("serial error")) return "ошибка порта";

  return status;
}

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function mapRange(value, inMin, inMax, outMin, outMax) {
  const v = clamp(value, inMin, inMax);
  return outMin + ((v - inMin) * (outMax - outMin)) / (inMax - inMin);
}

function fitCanvasToCssSize() {
  const rect = canvas.getBoundingClientRect();
  const scale = window.devicePixelRatio || 1;

  const targetWidth = Math.max(1, Math.floor(rect.width * scale));
  const targetHeight = Math.max(1, Math.floor(rect.height * scale));

  if (canvas.width !== targetWidth || canvas.height !== targetHeight) {
    canvas.width = targetWidth;
    canvas.height = targetHeight;
  }
}

function drawRadar(data) {
  fitCanvasToCssSize();

  const w = canvas.width;
  const h = canvas.height;
  const cx = w / 2;
  const cy = h / 2;
  const radius = Math.min(w, h) * 0.44;

  ctx.clearRect(0, 0, w, h);

  ctx.fillStyle = "#05070d";
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = "#243044";
  ctx.lineWidth = Math.max(1, w * 0.003);

  for (let i = 1; i <= 4; i++) {
    ctx.beginPath();
    ctx.arc(cx, cy, (radius * i) / 4, 0, Math.PI * 2);
    ctx.stroke();
  }

  ctx.strokeStyle = "#52617a";
  ctx.lineWidth = Math.max(1, w * 0.004);

  ctx.beginPath();
  ctx.moveTo(cx, cy - radius);
  ctx.lineTo(cx, cy + radius);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(cx - radius, cy);
  ctx.lineTo(cx + radius, cy);
  ctx.stroke();

  const px = cx + mapRange(data.x, -100, 100, -radius, radius);
  const py = cy - mapRange(data.y, -100, 100, -radius, radius);

  ctx.fillStyle = "#ffcf4a";
  ctx.beginPath();
  ctx.arc(px, py, Math.max(8, w * 0.026), 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = "#ffffff";
  ctx.lineWidth = Math.max(1, w * 0.004);
  ctx.beginPath();
  ctx.arc(px, py, Math.max(12, w * 0.036), 0, Math.PI * 2);
  ctx.stroke();

  ctx.fillStyle = "#dbe7ff";
  ctx.font = `${Math.max(14, w * 0.04)}px system-ui`;
  ctx.fillText(`X=${data.x}  Y=${data.y}`, Math.max(12, w * 0.04), Math.max(26, w * 0.075));

  ctx.fillStyle = "#8d99ad";
  ctx.font = `${Math.max(11, w * 0.03)}px system-ui`;
  ctx.fillText("М1 левый · М2 правый · М3 передний", Math.max(12, w * 0.04), h - Math.max(14, w * 0.04));
}

function updatePanel(data) {
  statusEl.textContent = translateStatus(data.status);
  xEl.textContent = data.x;
  yEl.textContent = data.y;

  for (let i = 0; i < 3; i++) {
    const freq = data.freq[i] ?? 0;
    const vol = data.vol[i] ?? 0;

    freqEls[i].textContent = freq;
    volEls[i].textContent = vol;

    freqBars[i].style.width = `${mapRange(freq, 100, 800, 0, 100)}%`;
    volBars[i].style.width = `${mapRange(vol, 0, 4095, 0, 100)}%`;
  }

  rawEl.textContent = JSON.stringify(data, null, 2);
}

function render() {
  drawRadar(latest);
  updatePanel(latest);
  requestAnimationFrame(render);
}

function connectWebSocket() {
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const url = `${protocol}://${window.location.host}/ws`;

  const ws = new WebSocket(url);

  ws.onopen = () => {
    statusEl.textContent = "подключено";
  };

  ws.onmessage = (event) => {
    try {
      latest = JSON.parse(event.data);
    } catch (err) {
      console.error("Ошибка данных WebSocket:", err);
    }
  };

  ws.onclose = () => {
    statusEl.textContent = "переподключение";
    setTimeout(connectWebSocket, 1000);
  };

  ws.onerror = () => {
    statusEl.textContent = "ошибка связи";
    ws.close();
  };
}

window.addEventListener("resize", fitCanvasToCssSize);

connectWebSocket();
render();
