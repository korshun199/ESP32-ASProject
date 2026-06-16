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
  status: "waiting",
};

function clamp(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function mapRange(value, inMin, inMax, outMin, outMax) {
  const v = clamp(value, inMin, inMax);
  return outMin + ((v - inMin) * (outMax - outMin)) / (inMax - inMin);
}

function drawGrid(data) {
  const w = canvas.width;
  const h = canvas.height;

  ctx.clearRect(0, 0, w, h);

  ctx.fillStyle = "#0c0f15";
  ctx.fillRect(0, 0, w, h);

  ctx.strokeStyle = "#263044";
  ctx.lineWidth = 1;

  for (let i = 0; i <= 10; i++) {
    const p = (i / 10) * w;

    ctx.beginPath();
    ctx.moveTo(p, 0);
    ctx.lineTo(p, h);
    ctx.stroke();

    ctx.beginPath();
    ctx.moveTo(0, p);
    ctx.lineTo(w, p);
    ctx.stroke();
  }

  ctx.strokeStyle = "#63708a";
  ctx.lineWidth = 2;

  ctx.beginPath();
  ctx.moveTo(w / 2, 0);
  ctx.lineTo(w / 2, h);
  ctx.stroke();

  ctx.beginPath();
  ctx.moveTo(0, h / 2);
  ctx.lineTo(w, h / 2);
  ctx.stroke();

  const px = mapRange(data.x, -100, 100, 0, w);
  const py = mapRange(data.y, -100, 100, h, 0);

  ctx.fillStyle = "#ffcf4a";
  ctx.beginPath();
  ctx.arc(px, py, 14, 0, Math.PI * 2);
  ctx.fill();

  ctx.strokeStyle = "#ffffff";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(px, py, 18, 0, Math.PI * 2);
  ctx.stroke();

  ctx.fillStyle = "#d7dee9";
  ctx.font = "18px system-ui";
  ctx.fillText(`X=${data.x} Y=${data.y}`, 16, 28);

  ctx.fillStyle = "#8fa0b8";
  ctx.fillText("MIC1 left · MIC2 right · MIC3 front", 16, h - 18);
}

function updatePanel(data) {
  statusEl.textContent = data.status || "online";
  xEl.textContent = data.x;
  yEl.textContent = data.y;

  for (let i = 0; i < 3; i++) {
    freqEls[i].textContent = data.freq[i] ?? 0;
    volEls[i].textContent = data.vol[i] ?? 0;

    const freqPercent = mapRange(data.freq[i] ?? 0, 100, 800, 0, 100);
    const volPercent = mapRange(data.vol[i] ?? 0, 0, 4095, 0, 100);

    freqBars[i].style.width = `${freqPercent}%`;
    volBars[i].style.width = `${volPercent}%`;
  }

  rawEl.textContent = JSON.stringify(data, null, 2);
}

function render() {
  drawGrid(latest);
  updatePanel(latest);
  requestAnimationFrame(render);
}

function connectWebSocket() {
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const url = `${protocol}://${window.location.host}/ws`;

  const ws = new WebSocket(url);

  ws.onopen = () => {
    statusEl.textContent = "websocket connected";
  };

  ws.onmessage = (event) => {
    try {
      latest = JSON.parse(event.data);
    } catch (err) {
      console.error("Bad websocket data:", err);
    }
  };

  ws.onclose = () => {
    statusEl.textContent = "websocket reconnecting";
    setTimeout(connectWebSocket, 1000);
  };

  ws.onerror = () => {
    ws.close();
  };
}

connectWebSocket();
render();
