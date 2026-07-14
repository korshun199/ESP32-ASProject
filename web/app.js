/*
  Клиент живой веб-панели ESP32 Audio Radar.
*/

const defaultSettings = {
  rmsMinimum: 5000,
  rmsMaximum: 700000,

  historySeconds: 30,
  levelSmoothing: 0.55,

  waveGain: 1,
  waveAutoScale: false,

  spectrumSmoothing: 0.65,
  spectrumGain: 1,
  frequencyMinimum: 125,
  frequencyMaximum: 7875,

  peakHoldEnabled: true,
  peakDecay: 18,

  spectrogramMinimum: 100,
  spectrogramMaximum: 900,
  spectrogramGain: 1
};

const settingsKey =
  "esp32AudioRadarSettingsV2";

const elements = {
  connectionLamp:
    document.getElementById("connectionLamp"),

  connectionText:
    document.getElementById("connectionText"),

  levelValue:
    document.getElementById("levelValue"),

  levelMeterFill:
    document.getElementById("levelMeterFill"),

  rmsValue:
    document.getElementById("rmsValue"),

  peakValue:
    document.getElementById("peakValue"),

  p2pValue:
    document.getElementById("p2pValue"),

  frequencyValue:
    document.getElementById("frequencyValue"),

  fpsValue:
    document.getElementById("fpsValue"),

  frameValue:
    document.getElementById("frameValue"),

  meanValue:
    document.getElementById("meanValue"),

  minimumMaximumValue:
    document.getElementById("minimumMaximumValue"),

  receivedFramesValue:
    document.getElementById("receivedFramesValue"),

  webFpsValue:
    document.getElementById("webFpsValue"),

  historyRangeText:
    document.getElementById("historyRangeText"),

  waveScaleText:
    document.getElementById("waveScaleText"),

  spectrumRangeText:
    document.getElementById("spectrumRangeText"),

  spectrumMaximumText:
    document.getElementById("spectrumMaximumText"),

  spectrogramScaleText:
    document.getElementById("spectrogramScaleText"),

  pauseState:
    document.getElementById("pauseState"),

  historyCanvas:
    document.getElementById("historyCanvas"),

  waveCanvas:
    document.getElementById("waveCanvas"),

  spectrumCanvas:
    document.getElementById("spectrumCanvas"),

  spectrogramCanvas:
    document.getElementById("spectrogramCanvas"),

  rmsMinimum:
    document.getElementById("rmsMinimum"),

  rmsMaximum:
    document.getElementById("rmsMaximum"),

  historySeconds:
    document.getElementById("historySeconds"),

  historySecondsValue:
    document.getElementById("historySecondsValue"),

  levelSmoothing:
    document.getElementById("levelSmoothing"),

  levelSmoothingValue:
    document.getElementById("levelSmoothingValue"),

  waveGain:
    document.getElementById("waveGain"),

  waveGainValue:
    document.getElementById("waveGainValue"),

  waveAutoScale:
    document.getElementById("waveAutoScale"),

  spectrumSmoothing:
    document.getElementById("spectrumSmoothing"),

  spectrumSmoothingValue:
    document.getElementById("spectrumSmoothingValue"),

  frequencyMinimum:
    document.getElementById("frequencyMinimum"),

  frequencyMaximum:
    document.getElementById("frequencyMaximum"),

  spectrumGain:
    document.getElementById("spectrumGain"),

  spectrumGainValue:
    document.getElementById("spectrumGainValue"),

  peakHoldEnabled:
    document.getElementById("peakHoldEnabled"),

  peakDecay:
    document.getElementById("peakDecay"),

  peakDecayValue:
    document.getElementById("peakDecayValue"),

  spectrogramMinimum:
    document.getElementById("spectrogramMinimum"),

  spectrogramMinimumValue:
    document.getElementById("spectrogramMinimumValue"),

  spectrogramMaximum:
    document.getElementById("spectrogramMaximum"),

  spectrogramMaximumValue:
    document.getElementById("spectrogramMaximumValue"),

  spectrogramGain:
    document.getElementById("spectrogramGain"),

  spectrogramGainValue:
    document.getElementById("spectrogramGainValue"),

  pauseButton:
    document.getElementById("pauseButton"),

  clearButton:
    document.getElementById("clearButton"),

  resetButton:
    document.getElementById("resetButton")
};

let settings = loadSettings();

let websocket = null;
let reconnectTimer = null;

let paused = false;
let receivedFrames = 0;

let lastFrame = null;
let smoothedLevel = 0;
let smoothedSpectrum = [];
let peakSpectrum = [];

let levelHistory = [];
let spectrumHistory = [];

let webFpsFrames = 0;
let webFpsStartedAt = performance.now();
let webFps = 0;


function loadSettings() {
  try {
    const saved =
      JSON.parse(
        localStorage.getItem(settingsKey)
      );

    return {
      ...defaultSettings,
      ...saved
    };
  } catch {
    return {
      ...defaultSettings
    };
  }
}


function saveSettings() {
  localStorage.setItem(
    settingsKey,
    JSON.stringify(settings)
  );
}


function formatNumber(value) {
  return new Intl.NumberFormat(
    "ru-RU"
  ).format(
    Math.round(value || 0)
  );
}


function clamp(
  value,
  minimum,
  maximum
) {
  return Math.max(
    minimum,
    Math.min(
      maximum,
      value
    )
  );
}


function calculateLevel(rms) {
  const minimum =
    Math.max(
      1,
      settings.rmsMinimum
    );

  const maximum =
    Math.max(
      minimum + 1,
      settings.rmsMaximum
    );

  if (rms <= minimum) {
    return 0;
  }

  if (rms >= maximum) {
    return 100;
  }

  const current =
    20 *
    Math.log10(
      rms / minimum
    );

  const full =
    20 *
    Math.log10(
      maximum / minimum
    );

  return clamp(
    current / full * 100,
    0,
    100
  );
}


function applySettingsToControls() {
  elements.rmsMinimum.value =
    settings.rmsMinimum;

  elements.rmsMaximum.value =
    settings.rmsMaximum;

  elements.historySeconds.value =
    settings.historySeconds;

  elements.levelSmoothing.value =
    settings.levelSmoothing;

  elements.waveGain.value =
    settings.waveGain;

  elements.waveAutoScale.checked =
    settings.waveAutoScale;

  elements.spectrumSmoothing.value =
    settings.spectrumSmoothing;

  elements.frequencyMinimum.value =
    settings.frequencyMinimum;

  elements.frequencyMaximum.value =
    settings.frequencyMaximum;

  elements.spectrumGain.value =
    settings.spectrumGain;

  elements.peakHoldEnabled.checked =
    settings.peakHoldEnabled;

  elements.peakDecay.value =
    settings.peakDecay;

  elements.spectrogramMinimum.value =
    settings.spectrogramMinimum;

  elements.spectrogramMaximum.value =
    settings.spectrogramMaximum;

  elements.spectrogramGain.value =
    settings.spectrogramGain;

  updateSettingsLabels();
}


function updateSettingsLabels() {
  elements.historySecondsValue.textContent =
    `${settings.historySeconds} с`;

  elements.levelSmoothingValue.textContent =
    `${Math.round(
      settings.levelSmoothing * 100
    )}%`;

  elements.waveGainValue.textContent =
    `${Number(
      settings.waveGain
    ).toFixed(1)}×`;

  elements.spectrumSmoothingValue.textContent =
    `${Math.round(
      settings.spectrumSmoothing * 100
    )}%`;

  elements.spectrumGainValue.textContent =
    `${Number(
      settings.spectrumGain
    ).toFixed(1)}×`;

  elements.peakDecayValue.textContent =
    `${settings.peakDecay}`;

  elements.spectrogramMinimumValue.textContent =
    `${settings.spectrogramMinimum}`;

  elements.spectrogramMaximumValue.textContent =
    `${settings.spectrogramMaximum}`;

  elements.spectrogramGainValue.textContent =
    `${Number(
      settings.spectrogramGain
    ).toFixed(1)}×`;

  elements.historyRangeText.textContent =
    `${settings.historySeconds} секунд`;

  elements.waveScaleText.textContent =
    settings.waveAutoScale
      ? "AUTO"
      : `×${Number(
          settings.waveGain
        ).toFixed(1)}`;

  elements.spectrumRangeText.textContent =
    `${settings.frequencyMinimum}–${settings.frequencyMaximum} Гц`;

  elements.spectrogramScaleText.textContent =
    `${settings.spectrogramMinimum}–${settings.spectrogramMaximum}`;
}


function readSettingsFromControls() {
  settings.rmsMinimum =
    Number(
      elements.rmsMinimum.value
    );

  settings.rmsMaximum =
    Number(
      elements.rmsMaximum.value
    );

  settings.historySeconds =
    Number(
      elements.historySeconds.value
    );

  settings.levelSmoothing =
    Number(
      elements.levelSmoothing.value
    );

  settings.waveGain =
    Number(
      elements.waveGain.value
    );

  settings.waveAutoScale =
    elements.waveAutoScale.checked;

  settings.spectrumSmoothing =
    Number(
      elements.spectrumSmoothing.value
    );

  settings.frequencyMinimum =
    Number(
      elements.frequencyMinimum.value
    );

  settings.frequencyMaximum =
    Number(
      elements.frequencyMaximum.value
    );

  settings.spectrumGain =
    Number(
      elements.spectrumGain.value
    );

  settings.peakHoldEnabled =
    elements.peakHoldEnabled.checked;

  settings.peakDecay =
    Number(
      elements.peakDecay.value
    );

  settings.spectrogramMinimum =
    Number(
      elements.spectrogramMinimum.value
    );

  settings.spectrogramMaximum =
    Number(
      elements.spectrogramMaximum.value
    );

  settings.spectrogramGain =
    Number(
      elements.spectrogramGain.value
    );

  if (
    settings.rmsMaximum <=
    settings.rmsMinimum
  ) {
    settings.rmsMaximum =
      settings.rmsMinimum + 1000;

    elements.rmsMaximum.value =
      settings.rmsMaximum;
  }

  if (
    settings.frequencyMaximum <=
    settings.frequencyMinimum
  ) {
    settings.frequencyMaximum =
      settings.frequencyMinimum + 250;

    elements.frequencyMaximum.value =
      settings.frequencyMaximum;
  }

  if (
    settings.spectrogramMaximum <=
    settings.spectrogramMinimum
  ) {
    settings.spectrogramMaximum =
      Math.min(
        1000,
        settings.spectrogramMinimum + 100
      );

    elements.spectrogramMaximum.value =
      settings.spectrogramMaximum;
  }

  trimHistory();
  saveSettings();
  updateSettingsLabels();

  if (lastFrame) {
    renderAll(lastFrame, false);
  }
}


function setConnectionState(
  connected,
  text
) {
  elements.connectionLamp.classList.toggle(
    "connected",
    connected
  );

  elements.connectionLamp.classList.toggle(
    "disconnected",
    !connected
  );

  elements.connectionText.textContent =
    text;
}


function resizeCanvas(canvas) {
  const ratio =
    window.devicePixelRatio || 1;

  const width =
    Math.max(
      1,
      canvas.clientWidth
    );

  const height =
    Math.max(
      1,
      canvas.clientHeight
    );

  const targetWidth =
    Math.round(
      width * ratio
    );

  const targetHeight =
    Math.round(
      height * ratio
    );

  if (
    canvas.width !== targetWidth ||
    canvas.height !== targetHeight
  ) {
    canvas.width =
      targetWidth;

    canvas.height =
      targetHeight;
  }

  return {
    width: targetWidth,
    height: targetHeight,
    ratio
  };
}


function drawGrid(
  context,
  width,
  height,
  verticalLines = 10,
  horizontalLines = 5
) {
  context.clearRect(
    0,
    0,
    width,
    height
  );

  context.strokeStyle =
    "rgba(143, 164, 191, 0.15)";

  context.lineWidth = 1;

  for (
    let index = 1;
    index < verticalLines;
    index++
  ) {
    const x =
      width *
      index /
      verticalLines;

    context.beginPath();
    context.moveTo(x, 0);
    context.lineTo(x, height);
    context.stroke();
  }

  for (
    let index = 1;
    index < horizontalLines;
    index++
  ) {
    const y =
      height *
      index /
      horizontalLines;

    context.beginPath();
    context.moveTo(0, y);
    context.lineTo(width, y);
    context.stroke();
  }
}


function drawAxisText(
  context,
  text,
  x,
  y,
  ratio,
  align = "left"
) {
  context.fillStyle =
    "rgba(231, 238, 248, 0.72)";

  context.font =
    `${11 * ratio}px sans-serif`;

  context.textAlign =
    align;

  context.fillText(
    text,
    x,
    y
  );

  context.textAlign =
    "left";
}


function trimHistory() {
  const maximumPoints =
    Math.max(
      20,
      Math.round(
        settings.historySeconds *
        8
      )
    );

  if (
    levelHistory.length >
    maximumPoints
  ) {
    levelHistory =
      levelHistory.slice(
        -maximumPoints
      );
  }

  if (
    spectrumHistory.length >
    maximumPoints
  ) {
    spectrumHistory =
      spectrumHistory.slice(
        -maximumPoints
      );
  }
}


function drawHistory() {
  const canvas =
    elements.historyCanvas;

  const context =
    canvas.getContext("2d");

  const size =
    resizeCanvas(canvas);

  drawGrid(
    context,
    size.width,
    size.height,
    10,
    4
  );

  drawAxisText(
    context,
    "100%",
    7 * size.ratio,
    15 * size.ratio,
    size.ratio
  );

  drawAxisText(
    context,
    "0%",
    7 * size.ratio,
    size.height -
      7 * size.ratio,
    size.ratio
  );

  if (
    levelHistory.length < 2
  ) {
    return;
  }

  const step =
    size.width /
    Math.max(
      1,
      levelHistory.length - 1
    );

  context.beginPath();

  levelHistory.forEach(
    (item, index) => {
      const x =
        index * step;

      const y =
        size.height -
        clamp(
          item.level,
          0,
          100
        ) /
        100 *
        size.height;

      if (index === 0) {
        context.moveTo(x, y);
      } else {
        context.lineTo(x, y);
      }
    }
  );

  const gradient =
    context.createLinearGradient(
      0,
      0,
      size.width,
      0
    );

  gradient.addColorStop(
    0,
    "#4da3ff"
  );

  gradient.addColorStop(
    0.65,
    "#55d6be"
  );

  gradient.addColorStop(
    0.86,
    "#ffd166"
  );

  gradient.addColorStop(
    1,
    "#ff5d73"
  );

  context.strokeStyle =
    gradient;

  context.lineWidth =
    Math.max(
      1,
      2 * size.ratio
    );

  context.stroke();

  context.lineTo(
    size.width,
    size.height
  );

  context.lineTo(
    0,
    size.height
  );

  context.closePath();

  context.globalAlpha =
    0.10;

  context.fillStyle =
    "#55d6be";

  context.fill();

  context.globalAlpha =
    1;
}


function drawWave(wave) {
  const canvas =
    elements.waveCanvas;

  const context =
    canvas.getContext("2d");

  const size =
    resizeCanvas(canvas);

  drawGrid(
    context,
    size.width,
    size.height,
    10,
    6
  );

  const middle =
    size.height / 2;

  context.strokeStyle =
    "rgba(85, 214, 190, 0.38)";

  context.beginPath();
  context.moveTo(0, middle);
  context.lineTo(
    size.width,
    middle
  );
  context.stroke();

  if (
    !wave ||
    wave.length < 2
  ) {
    return;
  }

  let gain =
    Number(
      settings.waveGain
    );

  if (settings.waveAutoScale) {
    const maximum =
      Math.max(
        1,
        ...wave.map(
          value =>
            Math.abs(value)
        )
      );

    gain =
      900 / maximum;
  }

  context.strokeStyle =
    "#55d6be";

  context.lineWidth =
    Math.max(
      1,
      size.ratio * 1.6
    );

  context.beginPath();

  wave.forEach(
    (value, index) => {
      const x =
        index /
        (wave.length - 1) *
        size.width;

      const normalized =
        clamp(
          value /
          1000 *
          gain,
          -1,
          1
        );

      const y =
        middle -
        normalized *
        size.height *
        0.44;

      if (index === 0) {
        context.moveTo(x, y);
      } else {
        context.lineTo(x, y);
      }
    }
  );

  context.stroke();

  drawAxisText(
    context,
    "+",
    7 * size.ratio,
    15 * size.ratio,
    size.ratio
  );

  drawAxisText(
    context,
    "−",
    7 * size.ratio,
    size.height -
      7 * size.ratio,
    size.ratio
  );
}


function visibleSpectrum(frame) {
  const result = [];

  smoothedSpectrum.forEach(
    (value, index) => {
      const frequency =
        frame.spectrum_first_hz +
        index *
        frame.spectrum_step_hz;

      if (
        frequency >=
          settings.frequencyMinimum &&
        frequency <=
          settings.frequencyMaximum
      ) {
        result.push({
          index,
          frequency,
          value
        });
      }
    }
  );

  return result;
}


function updateSpectrumData(frame) {
  const spectrum =
    frame.spectrum || [];

  if (
    smoothedSpectrum.length !==
    spectrum.length
  ) {
    smoothedSpectrum =
      [...spectrum];

    peakSpectrum =
      [...spectrum];
  }

  const smoothing =
    settings.spectrumSmoothing;

  smoothedSpectrum =
    spectrum.map(
      (value, index) =>
        smoothedSpectrum[index] *
        smoothing +
        value *
        (1 - smoothing)
    );

  peakSpectrum =
    smoothedSpectrum.map(
      (value, index) =>
        Math.max(
          value,
          (
            peakSpectrum[index] || 0
          ) -
          settings.peakDecay
        )
    );
}


function drawSpectrum(frame) {
  const canvas =
    elements.spectrumCanvas;

  const context =
    canvas.getContext("2d");

  const size =
    resizeCanvas(canvas);

  drawGrid(
    context,
    size.width,
    size.height,
    10,
    5
  );

  const bands =
    visibleSpectrum(frame);

  if (!bands.length) {
    return;
  }

  const gap =
    Math.max(
      1,
      size.width * 0.002
    );

  const barWidth =
    size.width /
    bands.length;

  bands.forEach(
    (band, visibleIndex) => {
      const normalized =
        clamp(
          band.value /
          1000 *
          settings.spectrumGain,
          0,
          1
        );

      const height =
        normalized *
        size.height *
        0.88;

      const x =
        visibleIndex *
        barWidth +
        gap;

      const y =
        size.height -
        height;

      const hue =
        205 -
        normalized * 170;

      context.fillStyle =
        `hsl(${hue} 85% 58%)`;

      context.fillRect(
        x,
        y,
        Math.max(
          1,
          barWidth -
          gap * 2
        ),
        height
      );

      if (
        settings.peakHoldEnabled
      ) {
        const peakNormalized =
          clamp(
            (
              peakSpectrum[
                band.index
              ] || 0
            ) /
            1000 *
            settings.spectrumGain,
            0,
            1
          );

        const peakY =
          size.height -
          peakNormalized *
          size.height *
          0.88;

        context.fillStyle =
          "#ffffff";

        context.fillRect(
          x,
          peakY,
          Math.max(
            1,
            barWidth -
            gap * 2
          ),
          Math.max(
            1,
            size.ratio
          )
        );
      }
    }
  );

  const labelCount =
    Math.min(
      6,
      bands.length
    );

  for (
    let labelIndex = 0;
    labelIndex < labelCount;
    labelIndex++
  ) {
    const bandIndex =
      Math.round(
        labelIndex *
        (bands.length - 1) /
        Math.max(
          1,
          labelCount - 1
        )
      );

    const band =
      bands[bandIndex];

    const x =
      bandIndex /
      Math.max(
        1,
        bands.length - 1
      ) *
      size.width;

    drawAxisText(
      context,
      `${Math.round(
        band.frequency
      )}`,
      clamp(
        x,
        24 * size.ratio,
        size.width -
          24 * size.ratio
      ),
      15 * size.ratio,
      size.ratio,
      "center"
    );
  }
}


function spectrogramColor(value) {
  const minimum =
    settings.spectrogramMinimum;

  const maximum =
    Math.max(
      minimum + 1,
      settings.spectrogramMaximum
    );

  const normalized =
    clamp(
      (
        value *
        settings.spectrogramGain -
        minimum
      ) /
      (
        maximum -
        minimum
      ),
      0,
      1
    );

  const stops = [
    [0.00, [3, 7, 17]],
    [0.18, [23, 63, 143]],
    [0.38, [0, 168, 168]],
    [0.58, [138, 211, 60]],
    [0.76, [255, 209, 102]],
    [0.91, [255, 93, 115]],
    [1.00, [255, 255, 255]]
  ];

  for (
    let index = 1;
    index < stops.length;
    index++
  ) {
    const previous =
      stops[index - 1];

    const current =
      stops[index];

    if (
      normalized <=
      current[0]
    ) {
      const position =
        (
          normalized -
          previous[0]
        ) /
        (
          current[0] -
          previous[0]
        );

      const color =
        previous[1].map(
          (component, componentIndex) =>
            Math.round(
              component +
              (
                current[1][
                  componentIndex
                ] -
                component
              ) *
              position
            )
        );

      return `rgb(${color[0]} ${color[1]} ${color[2]})`;
    }
  }

  return "rgb(255 255 255)";
}


function drawSpectrogram(frame) {
  const canvas =
    elements.spectrogramCanvas;

  const context =
    canvas.getContext("2d");

  const size =
    resizeCanvas(canvas);

  context.clearRect(
    0,
    0,
    size.width,
    size.height
  );

  if (
    !spectrumHistory.length
  ) {
    return;
  }

  const columnWidth =
    size.width /
    spectrumHistory.length;

  spectrumHistory.forEach(
    (historyItem, columnIndex) => {
      const spectrum =
        historyItem.spectrum;

      const visible = [];

      spectrum.forEach(
        (value, index) => {
          const frequency =
            historyItem.firstHz +
            index *
            historyItem.stepHz;

          if (
            frequency >=
              settings.frequencyMinimum &&
            frequency <=
              settings.frequencyMaximum
          ) {
            visible.push(value);
          }
        }
      );

      if (!visible.length) {
        return;
      }

      const rowHeight =
        size.height /
        visible.length;

      visible.forEach(
        (value, rowIndex) => {
          const y =
            size.height -
            (
              rowIndex + 1
            ) *
            rowHeight;

          context.fillStyle =
            spectrogramColor(
              value
            );

          context.fillRect(
            columnIndex *
            columnWidth,
            y,
            Math.ceil(
              columnWidth + 0.5
            ),
            Math.ceil(
              rowHeight + 0.5
            )
          );
        }
      );
    }
  );

  context.strokeStyle =
    "rgba(255, 255, 255, 0.16)";

  context.lineWidth = 1;

  for (
    let index = 1;
    index < 5;
    index++
  ) {
    const y =
      size.height *
      index /
      5;

    context.beginPath();
    context.moveTo(0, y);
    context.lineTo(
      size.width,
      y
    );
    context.stroke();
  }

  drawAxisText(
    context,
    `${settings.frequencyMaximum} Гц`,
    7 * size.ratio,
    15 * size.ratio,
    size.ratio
  );

  drawAxisText(
    context,
    `${settings.frequencyMinimum} Гц`,
    7 * size.ratio,
    size.height -
      7 * size.ratio,
    size.ratio
  );
}


function updateCards(frame) {
  const rawLevel =
    calculateLevel(
      frame.rms
    );

  smoothedLevel =
    smoothedLevel *
    settings.levelSmoothing +
    rawLevel *
    (
      1 -
      settings.levelSmoothing
    );

  const level =
    clamp(
      smoothedLevel,
      0,
      100
    );

  elements.levelValue.textContent =
    `${Math.round(level)}%`;

  elements.levelMeterFill.style.width =
    `${level}%`;

  elements.rmsValue.textContent =
    formatNumber(
      frame.rms
    );

  elements.peakValue.textContent =
    formatNumber(
      frame.peak
    );

  elements.p2pValue.textContent =
    formatNumber(
      frame.p2p
    );

  elements.frequencyValue.textContent =
    `${Math.round(
      frame.dominant_hz || 0
    )} Гц`;

  elements.fpsValue.textContent =
    `${Number(
      frame.fps || 0
    ).toFixed(1)} / ${frame.read_errors}`;

  elements.frameValue.textContent =
    formatNumber(
      frame.id
    );

  elements.meanValue.textContent =
    formatNumber(
      frame.mean
    );

  elements.minimumMaximumValue.textContent =
    `${formatNumber(
      frame.min
    )} / ${formatNumber(
      frame.max
    )}`;

  elements.receivedFramesValue.textContent =
    formatNumber(
      receivedFrames
    );

  elements.webFpsValue.textContent =
    webFps.toFixed(1);

  elements.spectrumMaximumText.textContent =
    `максимум ${formatNumber(
      frame.spectrum_maximum
    )}`;

  return level;
}


function updateWebFps() {
  webFpsFrames++;

  const now =
    performance.now();

  const elapsed =
    now -
    webFpsStartedAt;

  if (elapsed >= 1000) {
    webFps =
      webFpsFrames *
      1000 /
      elapsed;

    webFpsFrames = 0;
    webFpsStartedAt = now;
  }
}


function addHistory(
  frame,
  level
) {
  levelHistory.push({
    time: frame.t_ms,
    level,
    rms: frame.rms
  });

  spectrumHistory.push({
    time: frame.t_ms,
    spectrum: [
      ...smoothedSpectrum
    ],
    firstHz:
      frame.spectrum_first_hz,
    stepHz:
      frame.spectrum_step_hz
  });

  trimHistory();
}


function renderAll(
  frame,
  addToHistory = true
) {
  updateSpectrumData(frame);

  const level =
    updateCards(frame);

  if (addToHistory) {
    addHistory(
      frame,
      level
    );
  }

  drawHistory();
  drawWave(frame.wave);
  drawSpectrum(frame);
  drawSpectrogram(frame);
}


function renderFrame(frame) {
  if (paused) {
    return;
  }

  lastFrame = frame;
  receivedFrames++;

  updateWebFps();
  renderAll(frame, true);
}


function clearGraphs() {
  levelHistory = [];
  spectrumHistory = [];

  smoothedSpectrum = [];
  peakSpectrum = [];
  smoothedLevel = 0;

  lastFrame = null;

  drawHistory();
  drawWave([]);

  const emptyFrame = {
    spectrum: [],
    spectrum_first_hz: 0,
    spectrum_step_hz: 0
  };

  drawSpectrum(emptyFrame);
  drawSpectrogram(emptyFrame);
}


function connectWebSocket() {
  clearTimeout(
    reconnectTimer
  );

  const protocol =
    location.protocol === "https:"
      ? "wss"
      : "ws";

  websocket =
    new WebSocket(
      `${protocol}://${location.host}/ws`
    );

  setConnectionState(
    false,
    "Подключение"
  );

  websocket.addEventListener(
    "open",
    () => {
      setConnectionState(
        true,
        "WebSocket подключён"
      );
    }
  );

  websocket.addEventListener(
    "message",
    event => {
      let message;

      try {
        message =
          JSON.parse(
            event.data
          );
      } catch {
        return;
      }

      if (
        message.type ===
        "server_state"
      ) {
        setConnectionState(
          Boolean(
            message.serial_connected
          ),
          message.serial_connected
            ? "ESP32 подключён"
            : "ESP32 не подключён"
        );

        if (message.latest) {
          renderFrame(
            message.latest
          );
        }

        return;
      }

      if (
        message.type ===
        "audio_frame"
      ) {
        setConnectionState(
          true,
          "ESP32 передаёт данные"
        );

        renderFrame(message);
      }
    }
  );

  websocket.addEventListener(
    "close",
    () => {
      setConnectionState(
        false,
        "Соединение потеряно"
      );

      reconnectTimer =
        setTimeout(
          connectWebSocket,
          1500
        );
    }
  );

  websocket.addEventListener(
    "error",
    () => {
      setConnectionState(
        false,
        "Ошибка соединения"
      );
    }
  );
}


const controls = [
  elements.rmsMinimum,
  elements.rmsMaximum,
  elements.historySeconds,
  elements.levelSmoothing,
  elements.waveGain,
  elements.waveAutoScale,
  elements.spectrumSmoothing,
  elements.frequencyMinimum,
  elements.frequencyMaximum,
  elements.spectrumGain,
  elements.peakHoldEnabled,
  elements.peakDecay,
  elements.spectrogramMinimum,
  elements.spectrogramMaximum,
  elements.spectrogramGain
];

controls.forEach(
  control => {
    control.addEventListener(
      "input",
      readSettingsFromControls
    );

    control.addEventListener(
      "change",
      readSettingsFromControls
    );
  }
);


elements.pauseButton.addEventListener(
  "click",
  () => {
    paused = !paused;

    elements.pauseButton.textContent =
      paused
        ? "Продолжить"
        : "Пауза";

    elements.pauseState.textContent =
      paused
        ? "PAUSE"
        : "LIVE";

    elements.pauseState.classList.toggle(
      "paused",
      paused
    );
  }
);


elements.clearButton.addEventListener(
  "click",
  clearGraphs
);


elements.resetButton.addEventListener(
  "click",
  () => {
    settings = {
      ...defaultSettings
    };

    saveSettings();
    applySettingsToControls();

    levelHistory = [];
    spectrumHistory = [];
    smoothedSpectrum = [];
    peakSpectrum = [];
    smoothedLevel = 0;

    if (lastFrame) {
      renderAll(
        lastFrame,
        false
      );
    } else {
      clearGraphs();
    }
  }
);


window.addEventListener(
  "resize",
  () => {
    if (lastFrame) {
      renderAll(
        lastFrame,
        false
      );
    } else {
      clearGraphs();
    }
  }
);


applySettingsToControls();
clearGraphs();
connectWebSocket();
