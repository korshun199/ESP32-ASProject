/*
  Запись сеансов и экспорт данных радара.
*/

(() => {
  "use strict";

  const journalKey =
    "esp32AudioRadarRecordingJournal";

  const elements = {
    sessionName:
      document.getElementById("sessionName"),

    recorderState:
      document.getElementById("recorderState"),

    recordingTime:
      document.getElementById("recordingTime"),

    recordingFrames:
      document.getElementById("recordingFrames"),

    recordStartButton:
      document.getElementById("recordStartButton"),

    recordStopButton:
      document.getElementById("recordStopButton"),

    exportJsonButton:
      document.getElementById("exportJsonButton"),

    exportCsvButton:
      document.getElementById("exportCsvButton"),

    exportPngButton:
      document.getElementById("exportPngButton"),

    recorderMessage:
      document.getElementById("recorderMessage"),

    recordingJournal:
      document.getElementById("recordingJournal"),

    spectrogramCanvas:
      document.getElementById("spectrogramCanvas")
  };

  let websocket = null;
  let reconnectTimer = null;

  let recording = false;
  let sessionFrames = [];

  let sessionStartedAt = null;
  let sessionStoppedAt = null;

  let timerHandle = null;


  function twoDigits(value) {
    return String(value).padStart(
      2,
      "0"
    );
  }


  function safeFileName(value) {
    return value
      .trim()
      .replace(
        /[^a-zA-Zа-яА-ЯёЁ0-9_-]+/g,
        "_"
      )
      .replace(
        /^_+|_+$/g,
        ""
      )
      .slice(
        0,
        70
      ) || "audio_session";
  }


  function defaultSessionName() {
    const now = new Date();

    return [
      "session",
      now.getFullYear(),
      twoDigits(
        now.getMonth() + 1
      ),
      twoDigits(
        now.getDate()
      ),
      twoDigits(
        now.getHours()
      ),
      twoDigits(
        now.getMinutes()
      ),
      twoDigits(
        now.getSeconds()
      )
    ].join("-");
  }


  function formatDuration(
    milliseconds
  ) {
    const totalSeconds =
      Math.max(
        0,
        Math.floor(
          milliseconds / 1000
        )
      );

    const hours =
      Math.floor(
        totalSeconds / 3600
      );

    const minutes =
      Math.floor(
        (
          totalSeconds % 3600
        ) /
        60
      );

    const seconds =
      totalSeconds % 60;

    if (hours > 0) {
      return [
        twoDigits(hours),
        twoDigits(minutes),
        twoDigits(seconds)
      ].join(":");
    }

    return [
      twoDigits(minutes),
      twoDigits(seconds)
    ].join(":");
  }


  function setMessage(
    text,
    type = ""
  ) {
    elements.recorderMessage.textContent =
      text;

    elements.recorderMessage.classList.remove(
      "error",
      "success"
    );

    if (type) {
      elements.recorderMessage.classList.add(
        type
      );
    }
  }


  function updateTimer() {
    if (!sessionStartedAt) {
      elements.recordingTime.textContent =
        "00:00";

      return;
    }

    const finish =
      recording
        ? Date.now()
        : sessionStoppedAt ||
          Date.now();

    elements.recordingTime.textContent =
      formatDuration(
        finish -
        sessionStartedAt
      );
  }


  function updateControls() {
    elements.recordStartButton.disabled =
      recording;

    elements.recordStopButton.disabled =
      !recording;

    const hasFrames =
      sessionFrames.length > 0;

    elements.exportJsonButton.disabled =
      !hasFrames;

    elements.exportCsvButton.disabled =
      !hasFrames;

    elements.recordingFrames.textContent =
      new Intl.NumberFormat(
        "ru-RU"
      ).format(
        sessionFrames.length
      );

    elements.recorderState.textContent =
      recording
        ? "ЗАПИСЬ"
        : hasFrames
          ? "ЗАПИСАНО"
          : "ГОТОВ";

    elements.recorderState.classList.toggle(
      "recording",
      recording
    );
  }


  function downloadBlob(
    content,
    mimeType,
    fileName
  ) {
    const blob =
      content instanceof Blob
        ? content
        : new Blob(
            [content],
            {
              type: mimeType
            }
          );

    const url =
      URL.createObjectURL(blob);

    const link =
      document.createElement("a");

    link.href = url;
    link.download = fileName;

    document.body.appendChild(link);
    link.click();
    link.remove();

    setTimeout(
      () => {
        URL.revokeObjectURL(url);
      },
      1000
    );
  }


  function getSessionName() {
    const name =
      elements.sessionName.value.trim();

    return name || defaultSessionName();
  }


  function startRecording() {
    sessionFrames = [];

    sessionStartedAt =
      Date.now();

    sessionStoppedAt =
      null;

    recording = true;

    if (
      !elements.sessionName.value.trim()
    ) {
      elements.sessionName.value =
        defaultSessionName();
    }

    clearInterval(timerHandle);

    timerHandle =
      setInterval(
        updateTimer,
        250
      );

    updateTimer();
    updateControls();

    setMessage(
      "Запись началась. Сохраняются все новые кадры WebSocket.",
      "success"
    );
  }


  function stopRecording() {
    if (!recording) {
      return;
    }

    recording = false;

    sessionStoppedAt =
      Date.now();

    clearInterval(timerHandle);
    timerHandle = null;

    updateTimer();
    updateControls();
    saveJournalEntry();

    setMessage(
      `Запись завершена: ${sessionFrames.length} кадров.`,
      "success"
    );
  }


  function sessionMetadata() {
    return {
      format:
        "ESP32 Audio Radar Session",

      format_version:
        1,

      name:
        getSessionName(),

      created_at:
        new Date().toISOString(),

      started_at:
        sessionStartedAt
          ? new Date(
              sessionStartedAt
            ).toISOString()
          : null,

      stopped_at:
        sessionStoppedAt
          ? new Date(
              sessionStoppedAt
            ).toISOString()
          : null,

      duration_ms:
        sessionStartedAt
          ? (
              sessionStoppedAt ||
              Date.now()
            ) -
            sessionStartedAt
          : 0,

      frame_count:
        sessionFrames.length,

      source:
        location.origin,

      browser:
        navigator.userAgent
    };
  }


  function exportJson() {
    if (!sessionFrames.length) {
      setMessage(
        "Для экспорта сначала нужна запись.",
        "error"
      );

      return;
    }

    const documentData = {
      metadata:
        sessionMetadata(),

      frames:
        sessionFrames
    };

    const fileName =
      `${safeFileName(
        getSessionName()
      )}.json`;

    downloadBlob(
      JSON.stringify(
        documentData,
        null,
        2
      ),
      "application/json;charset=utf-8",
      fileName
    );

    setMessage(
      `JSON сохранён: ${fileName}`,
      "success"
    );
  }


  function csvCell(value) {
    const text =
      value === null ||
      value === undefined
        ? ""
        : String(value);

    if (
      /[",\n\r;]/.test(text)
    ) {
      return `"${text.replace(
        /"/g,
        '""'
      )}"`;
    }

    return text;
  }


  function exportCsv() {
    if (!sessionFrames.length) {
      setMessage(
        "Для экспорта сначала нужна запись.",
        "error"
      );

      return;
    }

    const maximumSpectrumBands =
      Math.max(
        0,
        ...sessionFrames.map(
          frame =>
            Array.isArray(
              frame.spectrum
            )
              ? frame.spectrum.length
              : 0
        )
      );

    const header = [
      "index",
      "server_time",
      "t_ms",
      "frame_id",
      "count",
      "mean",
      "min",
      "max",
      "rms",
      "peak",
      "p2p",
      "level",
      "clip",
      "dominant_hz",
      "dominant_amplitude",
      "spectrum_maximum",
      "read_errors",
      "fps",
      "spectrum_first_hz",
      "spectrum_step_hz"
    ];

    for (
      let index = 0;
      index < maximumSpectrumBands;
      index++
    ) {
      header.push(
        `spectrum_${index}`
      );
    }

    const rows = [
      header.map(csvCell).join(",")
    ];

    sessionFrames.forEach(
      (frame, frameIndex) => {
        const row = [
          frameIndex,
          frame.server_time,
          frame.t_ms,
          frame.id,
          frame.count,
          frame.mean,
          frame.min,
          frame.max,
          frame.rms,
          frame.peak,
          frame.p2p,
          frame.level,
          frame.clip,
          frame.dominant_hz,
          frame.dominant_amplitude,
          frame.spectrum_maximum,
          frame.read_errors,
          frame.fps,
          frame.spectrum_first_hz,
          frame.spectrum_step_hz
        ];

        const spectrum =
          Array.isArray(
            frame.spectrum
          )
            ? frame.spectrum
            : [];

        for (
          let index = 0;
          index < maximumSpectrumBands;
          index++
        ) {
          row.push(
            spectrum[index] ?? ""
          );
        }

        rows.push(
          row.map(csvCell).join(",")
        );
      }
    );

    const fileName =
      `${safeFileName(
        getSessionName()
      )}.csv`;

    downloadBlob(
      "\uFEFF" +
      rows.join("\n"),
      "text/csv;charset=utf-8",
      fileName
    );

    setMessage(
      `CSV сохранён: ${fileName}`,
      "success"
    );
  }


  function exportSpectrogram() {
    const canvas =
      elements.spectrogramCanvas;

    if (
      !canvas ||
      canvas.width < 2 ||
      canvas.height < 2
    ) {
      setMessage(
        "Спектрограмма ещё не готова.",
        "error"
      );

      return;
    }

    const fileName =
      `${safeFileName(
        getSessionName()
      )}-spectrogram.png`;

    canvas.toBlob(
      blob => {
        if (!blob) {
          setMessage(
            "Не удалось создать PNG.",
            "error"
          );

          return;
        }

        downloadBlob(
          blob,
          "image/png",
          fileName
        );

        setMessage(
          `PNG сохранён: ${fileName}`,
          "success"
        );
      },
      "image/png"
    );
  }


  function readJournal() {
    try {
      const value =
        JSON.parse(
          localStorage.getItem(
            journalKey
          )
        );

      return Array.isArray(value)
        ? value
        : [];
    } catch {
      return [];
    }
  }


  function saveJournalEntry() {
    if (
      !sessionStartedAt ||
      !sessionFrames.length
    ) {
      return;
    }

    const journal =
      readJournal();

    journal.unshift({
      name:
        getSessionName(),

      stopped_at:
        new Date(
          sessionStoppedAt ||
          Date.now()
        ).toISOString(),

      duration_ms:
        (
          sessionStoppedAt ||
          Date.now()
        ) -
        sessionStartedAt,

      frame_count:
        sessionFrames.length
    });

    localStorage.setItem(
      journalKey,
      JSON.stringify(
        journal.slice(0, 10)
      )
    );

    renderJournal();
  }


  function renderJournal() {
    const journal =
      readJournal();

    elements.recordingJournal.replaceChildren();

    if (!journal.length) {
      const item =
        document.createElement("li");

      item.textContent =
        "Записей пока нет";

      elements.recordingJournal.appendChild(
        item
      );

      return;
    }

    journal.forEach(
      entry => {
        const item =
          document.createElement("li");

        const date =
          new Date(
            entry.stopped_at
          );

        item.textContent =
          `${entry.name}: ` +
          `${formatDuration(
            entry.duration_ms
          )}, ` +
          `${entry.frame_count} кадров, ` +
          `${date.toLocaleString(
            "ru-RU"
          )}`;

        elements.recordingJournal.appendChild(
          item
        );
      }
    );
  }


  function handleFrame(frame) {
    if (
      !recording ||
      frame.type !==
        "audio_frame"
    ) {
      return;
    }

    sessionFrames.push(
      structuredClone(frame)
    );

    updateControls();

    if (
      sessionFrames.length %
      25 ===
      0
    ) {
      setMessage(
        `Идёт запись: ${sessionFrames.length} кадров.`,
        "success"
      );
    }
  }


  function connectWebSocket() {
    clearTimeout(
      reconnectTimer
    );

    const protocol =
      location.protocol ===
      "https:"
        ? "wss"
        : "ws";

    websocket =
      new WebSocket(
        `${protocol}://${location.host}/ws`
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
          "audio_frame"
        ) {
          handleFrame(message);
        }
      }
    );

    websocket.addEventListener(
      "close",
      () => {
        if (recording) {
          setMessage(
            "WebSocket регистратора отключён. Ожидается переподключение.",
            "error"
          );
        }

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
        if (recording) {
          setMessage(
            "Ошибка WebSocket регистратора.",
            "error"
          );
        }
      }
    );
  }


  elements.recordStartButton.addEventListener(
    "click",
    startRecording
  );

  elements.recordStopButton.addEventListener(
    "click",
    stopRecording
  );

  elements.exportJsonButton.addEventListener(
    "click",
    exportJson
  );

  elements.exportCsvButton.addEventListener(
    "click",
    exportCsv
  );

  elements.exportPngButton.addEventListener(
    "click",
    exportSpectrogram
  );

  window.addEventListener(
    "beforeunload",
    event => {
      if (!recording) {
        return;
      }

      event.preventDefault();
      event.returnValue = "";
    }
  );

  elements.sessionName.value =
    defaultSessionName();

  renderJournal();
  updateTimer();
  updateControls();
  connectWebSocket();
})();
