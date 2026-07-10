/*
  ============================================================
  ПРОЕКТ РАДАР
  ============================================================

  Первая рабочая версия:
  ESP32-S3 + TFT ST7789 + цифровой I2S микрофон ICS-43434

  Что делает эта прошивка:

  1. Запускает TFT экран через правильный драйвер Adafruit_ST7789.
  2. Запускает цифровой микрофон ICS-43434 через I2S.
  3. Читает звук с микрофона.
  4. Считает примерную громкость.
  5. Показывает на экране:
     - заголовок SIMONG RADAR
     - рабочее поле RADAR
     - системный блок SYS
     - состояние микрофона MIC OK или NO
     - уровень громкости LVL в процентах
     - размах сигнала P2P

  Важное:

  Экран оказался НЕ ILI9341.
  Рабочий драйвер: Adafruit_ST7789.

  Рабочая инициализация экрана:

    tft.init(240, 320);
    tft.setRotation(0);

  Если снова заменить драйвер на ILI9341, экран опять начнёт вести себя
  как обиженный квадратный телевизор. Не надо так.

  ============================================================
  ПОДКЛЮЧЕНИЕ TFT
  ============================================================

  TFT VCC   -> 3V3
  TFT GND   -> GND
  TFT CS    -> GPIO10
  TFT DC    -> GPIO9
  TFT RESET -> GPIO8
  TFT SDI   -> GPIO11
  TFT SCK   -> GPIO12
  TFT LED   -> 3V3

  ============================================================
  ПОДКЛЮЧЕНИЕ МИКРОФОНА ICS-43434
  ============================================================

  MIC VDD / VCC  -> 3V3
  MIC GND        -> GND
  MIC SCK / BCLK -> GPIO4
  MIC WS / LRCLK -> GPIO5
  MIC SD / DOUT  -> GPIO6
  MIC L/R        -> GND

  L/R подключён к GND, поэтому микрофон отдаёт сигнал в левый канал.
  Поэтому в коде выбран режим I2S_CHANNEL_FMT_ONLY_LEFT.

  ============================================================
  ОБЩАЯ ИДЕЯ
  ============================================================

  setup:
    один раз запускает Serial, экран, разметку интерфейса и микрофон.

  loop:
    каждые 200 миллисекунд читает микрофон,
    пересчитывает громкость,
    обновляет значения на экране,
    пишет диагностику в Serial.

  Код специально написан простыми частями:

    - настройки пинов
    - настройки микрофона
    - расчёт размеров экрана
    - чтение микрофона
    - рисование статического интерфейса
    - рисование живых значений
    - setup и loop

  Чтобы будущий Олежка открыл файл и не захотел бить монитор.
  Хотя монитор иногда заслуживает.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "driver/i2s.h"

// ============================================================
// TFT PINS
// ============================================================
//
// Эти пины идут от ESP32-S3 к TFT экрану.
//
// CS   - выбор экрана на SPI-шине.
// DC   - говорит экрану, что сейчас идёт: команда или данные.
// RST  - аппаратный сброс экрана.
// MOSI - данные от ESP32 к экрану.
// SCLK - тактовый сигнал SPI.
//
// MISO не используется, потому что мы только пишем в экран,
// а не читаем из него.

#define TFT_CS    10
#define TFT_DC    9
#define TFT_RST   8
#define TFT_MOSI  11
#define TFT_SCLK  12

// Создаём объект экрана.
// Через него дальше будем рисовать текст, прямоугольники и индикаторы.

Adafruit_ST7789 tft(
  TFT_CS,
  TFT_DC,
  TFT_MOSI,
  TFT_SCLK,
  TFT_RST
);

// ============================================================
// MIC I2S PINS
// ============================================================
//
// ICS-43434 это цифровой микрофон.
// Он не отдаёт аналоговое напряжение.
// Он отдаёт цифровой поток данных по I2S.
//
// BCLK - битовый тактовый сигнал.
// WS   - выбор канала, левый или правый.
// DATA - сами звуковые данные от микрофона.

static const int I2S_BCLK_PIN = 4;
static const int I2S_WS_PIN   = 5;
static const int I2S_DATA_PIN = 6;

// ESP32 имеет несколько I2S-портов.
// Используем I2S_NUM_0, потому что нам одного микрофона достаточно.

static const i2s_port_t I2S_PORT = I2S_NUM_0;

// Частота дискретизации.
// 16000 означает 16000 измерений звука в секунду.
// Для простого индикатора громкости этого более чем достаточно.

static const int SAMPLE_RATE = 16000;

// Сколько сэмплов читаем за один раз.
// Чем больше число, тем стабильнее измерение,
// но тем медленнее обновление.
//
// 512 это нормальный компромисс:
// экран обновляется живо, а показания не совсем бешеные.

static const int READ_SAMPLES = 512;

// Границы для перевода громкости в проценты.
//
// LEVEL_MIN:
// ниже этого значения считаем почти тишиной.
//
// LEVEL_MAX:
// около этого значения считаем громкостью 100 процентов.
//
// Эти числа не физические децибелы.
// Это условная шкала для нашего индикатора.

static const int LEVEL_MIN = 5000;
static const int LEVEL_MAX = 80000;

// Буфер для сырых данных микрофона.
//
// Микрофон отдаёт 32-битные числа.
// Мы читаем сразу пачку значений в этот массив.

int32_t samples[READ_SAMPLES];

// ============================================================
// SCREEN SIZE
// ============================================================
//
// maxX и maxY это реальные размеры экрана,
// которые возвращает драйвер после init и rotation.
//
// Не пишем магические 240 и 320 по всему коду.
// Получили размер один раз, дальше строим интерфейс от него.

int maxX = 0;
int maxY = 0;

// ============================================================
// UI GEOMETRY
// ============================================================
//
// Здесь хранятся координаты прямоугольников интерфейса.
//
// Экран делится на:
//
// header  - верхняя красная полоса с названием.
// main    - основная область радара.
// monitor - правый системный блок с MIC, LVL, P2P.
// footer  - нижняя красная полоса со служебной информацией.
//
// X и Y это левый верхний угол блока.
// W это ширина.
// H это высота.

int headerX = 0;
int headerY = 0;
int headerW = 0;
int headerH = 0;

int mainX = 0;
int mainY = 0;
int mainW = 0;
int mainH = 0;

int monitorX = 0;
int monitorY = 0;
int monitorW = 0;
int monitorH = 0;

int footerX = 0;
int footerY = 0;
int footerW = 0;
int footerH = 0;

// ============================================================
// STATE
// ============================================================
//
// Эти переменные запоминают последние показанные значения.
//
// Зачем:
// чтобы не перерисовывать экран, если данные не изменились.
//
// Экран TFT не любит бессмысленную перерисовку.
// Если постоянно заливать одно и то же, будет мерцание и лишняя нагрузка.

int lastPercent = -1;
int32_t lastLevel = -1;
int32_t lastP2P = -1;

// ============================================================
// SERIAL HELPERS
// ============================================================
//
// printBoth печатает строку сразу в два Serial:
//
// Serial  - основной USB CDC.
// Serial0 - дополнительный аппаратный Serial.
//
// Это удобно при отладке, потому что на ESP32-S3 USB иногда ведёт себя
// как кот, который сам решает, будет ли он сегодня общаться.

void printBoth(const String &line) {
  Serial.println(line);
  Serial0.println(line);
}

// ============================================================
// SMALL HELPERS
// ============================================================

// Ограничивает число от 0 до 100.
//
// Если получилось меньше 0, возвращает 0.
// Если получилось больше 100, возвращает 100.
// Если число нормальное, возвращает как есть.

int clampPercent(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return v;
}

// Переводит внутренний уровень громкости в проценты.
//
// level это число, которое мы получили из микрофона.
// map переводит диапазон LEVEL_MIN ... LEVEL_MAX в 0 ... 100.
//
// Потом clampPercent страхует от выхода за границы.

int calcMicPercent(int32_t level) {
  return clampPercent(map(level, LEVEL_MIN, LEVEL_MAX, 0, 100));
}

// ============================================================
// LAYOUT
// ============================================================
//
// calcLayout рассчитывает положение всех блоков интерфейса.
//
// Главное:
// вся геометрия считается от maxX и maxY,
// а maxX и maxY берутся из tft.width и tft.height.
//
// Поэтому если экран повернуть или заменить на похожий,
// код хотя бы не будет сразу похож на школьную тетрадь после взрыва.

void calcLayout() {
  maxX = tft.width();
  maxY = tft.height();

  // Верхняя полоса:
  // занимает всю ширину экрана и одну восьмую высоты.

  headerX = 0;
  headerY = 0;
  headerW = maxX;
  headerH = maxY / 8;

  // Нижняя полоса:
  // тоже занимает всю ширину и одну восьмую высоты.

  footerX = 0;
  footerH = maxY / 8;
  footerY = maxY - footerH;
  footerW = maxX;

  // Правый системный блок:
  // занимает одну треть ширины экрана.
  // По высоте находится между header и footer.

  monitorW = maxX / 3;
  monitorH = maxY - headerH - footerH;
  monitorX = maxX - monitorW;
  monitorY = headerH;

  // Основная область радара:
  // занимает всё слева от системного блока.

  mainX = 0;
  mainY = headerH;
  mainW = maxX - monitorW;
  mainH = maxY - headerH - footerH;
}

// ============================================================
// I2S MICROPHONE SETUP
// ============================================================
//
// setupI2S включает I2S-приёмник ESP32.
//
// После этой функции ESP32 начинает принимать цифровой поток
// от микрофона ICS-43434.

void setupI2S() {
  // Основная конфигурация I2S.

  i2s_config_t cfg = {
    // MASTER:
    // ESP32 сам создаёт тактовые сигналы BCLK и WS.
    //
    // RX:
    // ESP32 только принимает звук, не передаёт.

    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),

    // Сколько измерений звука в секунду.

    .sample_rate = SAMPLE_RATE,

    // Микрофон отдаёт данные в 32-битном формате.

    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,

    // Читаем только левый канал,
    // потому что L/R на микрофоне подключён к GND.

    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,

    // Стандартный формат I2S для Arduino core 3.x.

    .communication_format = I2S_COMM_FORMAT_STAND_I2S,

    // Прерывания обычного уровня.

    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,

    // DMA-буферы.
    // Они нужны, чтобы звук принимался стабильно,
    // пока основной код занимается экраном.

    .dma_buf_count = 4,
    .dma_buf_len = 256,

    // APLL не используем.
    // Для индикатора громкости сверхточная аудиочастота не нужна.

    .use_apll = false,

    // Эти поля нужны структуре драйвера.
    // Для приёма звука они не играют главной роли.

    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  // Привязка I2S к реальным GPIO.

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_DATA_PIN
  };

  esp_err_t err;

  // Устанавливаем I2S-драйвер.

  err = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
  if (err != ESP_OK) {
    printBoth(String("mic=ERROR driver err=") + String((int)err));
    return;
  }

  // Назначаем пины I2S.

  err = i2s_set_pin(I2S_PORT, &pins);
  if (err != ESP_OK) {
    printBoth(String("mic=ERROR pins err=") + String((int)err));
    return;
  }

  // Очищаем DMA-буфер, чтобы на старте не читать мусор.

  i2s_zero_dma_buffer(I2S_PORT);

  printBoth("mic=I2S_READY");
}

// ============================================================
// READ MICROPHONE
// ============================================================
//
// readMic читает пачку сэмплов с микрофона и считает:
//
// percent - громкость в процентах для экрана.
// level   - усреднённая громкость после вычитания среднего.
// p2p     - peak-to-peak, разница между максимумом и минимумом.
//
// Возвращает:
// true  - данные прочитаны.
// false - данных нет или ошибка чтения.

bool readMic(int &percent, int32_t &level, int32_t &p2p) {
  size_t bytesRead = 0;

  // Читаем данные из I2S в массив samples.
  // Ждём максимум 100 миллисекунд.

  esp_err_t err = i2s_read(
    I2S_PORT,
    samples,
    sizeof(samples),
    &bytesRead,
    pdMS_TO_TICKS(100)
  );

  if (err != ESP_OK || bytesRead == 0) {
    return false;
  }

  // Сколько 32-битных чисел реально прочитали.

  int count = bytesRead / sizeof(int32_t);

  // Сумма нужна для среднего значения.
  // minVal и maxVal нужны для p2p.

  int64_t sum = 0;
  int32_t minVal = 2147483647;
  int32_t maxVal = -2147483647;

  for (int i = 0; i < count; i++) {
    // Микрофон отдаёт данные в старших битах.
    // Сдвиг вправо на 8 делает число удобнее для расчётов.

    int32_t v = samples[i] >> 8;

    sum += v;

    if (v < minVal) minVal = v;
    if (v > maxVal) maxVal = v;
  }

  // Среднее значение сигнала.
  //
  // Оно нужно, потому что цифровой микрофон может иметь смещение.
  // Нам важна не абсолютная высота сигнала, а колебания вокруг среднего.

  int32_t mean = sum / count;

  // Считаем среднее абсолютное отклонение от среднего.
  //
  // По-простому:
  // насколько сильно сигнал болтается вверх-вниз.
  // Чем сильнее звук, тем больше это число.

  int64_t sumCenteredAbs = 0;

  for (int i = 0; i < count; i++) {
    int32_t v = samples[i] >> 8;
    int32_t centered = v - mean;

    sumCenteredAbs += (centered < 0) ? -centered : centered;
  }

  level = sumCenteredAbs / count;

  // P2P показывает полный размах сигнала:
  // максимум минус минимум.

  p2p = maxVal - minVal;

  // Переводим level в проценты для экрана.

  percent = calcMicPercent(level);

  return true;
}

// ============================================================
// DRAW STATIC UI
// ============================================================
//
// Эти функции рисуют постоянные части интерфейса.
// Они вызываются один раз в setup.
//
// Живые значения микрофона рисуются отдельно,
// чтобы не перерисовывать весь экран каждые 200 мс.

void drawHeader() {
  // Красная верхняя полоса.

  tft.fillRect(headerX, headerY, headerW, headerH, ST77XX_RED);

  // Название проекта.

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(headerX + 8, headerY + 8);
  tft.print("SIMONG RADAR");
}

void drawMainArea() {
  // Основная чёрная область радара.

  tft.fillRect(mainX, mainY, mainW, mainH, ST77XX_BLACK);

  // Белая рамка основной области.

  tft.drawRect(mainX, mainY, mainW, mainH, ST77XX_WHITE);

  // Пока здесь просто надпись RADAR.
  // Во второй серии сюда можно добавлять графику радара.

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(mainX + 8, mainY + 8);
  tft.print("RADAR");
}

void drawMonitorFrame() {
  // Правая системная панель.

  tft.fillRect(monitorX, monitorY, monitorW, monitorH, ST77XX_BLACK);
  tft.drawRect(monitorX, monitorY, monitorW, monitorH, ST77XX_WHITE);

  // Заголовок SYS.

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setCursor(monitorX + 8, monitorY + 8);
  tft.print("SYS");

  // Подписи параметров.

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);

  tft.setCursor(monitorX + 8, monitorY + 42);
  tft.print("MIC");

  tft.setCursor(monitorX + 8, monitorY + 72);
  tft.print("LVL");

  tft.setCursor(monitorX + 8, monitorY + 102);
  tft.print("P2P");

  // Маленькая подпись платы снизу панели.

  tft.setCursor(monitorX + 8, monitorY + monitorH - 18);
  tft.print("S3");
}

void drawFooter() {
  // Нижняя красная полоса.

  tft.fillRect(footerX, footerY, footerW, footerH, ST77XX_RED);

  // Служебная информация:
  // драйвер и размер экрана, который увидел код.

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE, ST77XX_RED);
  tft.setCursor(footerX + 8, footerY + 8);
  tft.print("ST7789 ");
  tft.print(maxX);
  tft.print("x");
  tft.print(maxY);
}

void drawScreen() {
  // Очищаем экран и рисуем весь статический интерфейс.

  tft.fillScreen(ST77XX_BLACK);

  drawHeader();
  drawMainArea();
  drawMonitorFrame();
  drawFooter();
}

// ============================================================
// DRAW LIVE MONITOR VALUES
// ============================================================
//
// Эти функции обновляют только значения в правой панели.

void clearMonitorValues() {
  // Чистим только место, где находятся живые значения.
  // Подписи MIC, LVL, P2P не стираем.

  int x = monitorX + 38;
  int y = monitorY + 38;
  int w = monitorW - 42;
  int h = 90;

  if (w < 10) w = 10;

  tft.fillRect(x, y, w, h, ST77XX_BLACK);
}

void drawMicNoData() {
  // Показывает, что данных с микрофона нет.

  clearMonitorValues();

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_YELLOW, ST77XX_BLACK);

  tft.setCursor(monitorX + 42, monitorY + 42);
  tft.print("NO");

  tft.setCursor(monitorX + 42, monitorY + 72);
  tft.print("--");

  tft.setCursor(monitorX + 42, monitorY + 102);
  tft.print("--");
}

void drawMicValues(int percent, int32_t level, int32_t p2p) {
  // Если значения не изменились, ничего не рисуем.
  // Экрану меньше работы, глазам меньше мерцания.

  if (percent == lastPercent && level == lastLevel && p2p == lastP2P) {
    return;
  }

  // Запоминаем новые значения.

  lastPercent = percent;
  lastLevel = level;
  lastP2P = p2p;

  clearMonitorValues();

  // MIC OK.

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN, ST77XX_BLACK);

  tft.setCursor(monitorX + 42, monitorY + 42);
  tft.print("OK");

  // LVL в процентах.

  tft.setCursor(monitorX + 42, monitorY + 72);
  tft.print(percent);
  tft.print("%");

  // P2P.
  // Если число слишком большое, показываем укороченно.

  tft.setCursor(monitorX + 42, monitorY + 102);

  if (p2p > 999999) {
    tft.print("999K");
  } else if (p2p > 9999) {
    tft.print(p2p / 1000);
    tft.print("K");
  } else {
    tft.print(p2p);
  }

  // Полоска громкости.

  int barX = monitorX + 8;
  int barY = monitorY + 130;
  int barW = monitorW - 16;
  int barH = 10;

  // Если панель слишком узкая, полоску не рисуем.
  // Сейчас экран нормальный, но защита пусть будет.

  if (barW < 20) return;

  // Ширина зелёной части полоски.

  int fillW = map(percent, 0, 100, 0, barW - 2);

  // Рамка полоски.

  tft.drawRect(barX, barY, barW, barH, ST77XX_WHITE);

  // Чистим внутренность.

  tft.fillRect(barX + 1, barY + 1, barW - 2, barH - 2, ST77XX_BLACK);

  // Рисуем зелёную часть.

  tft.fillRect(barX + 1, barY + 1, fillW, barH - 2, ST77XX_GREEN);
}

// ============================================================
// SETUP
// ============================================================
//
// setup выполняется один раз после старта ESP32.
//
// Здесь поднимаем:
// - Serial
// - экран
// - разметку интерфейса
// - микрофон

void setup() {
  Serial.begin(115200);
  Serial0.begin(115200);

  // Небольшая пауза, чтобы USB Serial успел подняться.

  delay(1200);

  printBoth("");
  printBoth("# SIMONG RADAR 2026");
  printBoth("# ESP32-S3 + ST7789 + ICS-43434");

  // Запуск экрана.
  //
  // 240 на 320 это рабочий размер для нашего ST7789.
  // rotation 0 оказался правильным на реальном железе.

  tft.init(240, 320);
  tft.setRotation(0);

  // Считаем размеры и координаты интерфейса.

  calcLayout();

  printBoth(String("# tft=") + String(maxX) + "x" + String(maxY));

  // Рисуем экран.

  drawScreen();

  // Пока микрофон ещё не прочитан, показываем NO.

  drawMicNoData();

  // Запускаем микрофон.

  setupI2S();

  printBoth("# screen drawn");
}

// ============================================================
// LOOP
// ============================================================
//
// loop крутится бесконечно.
//
// Каждые 200 миллисекунд:
// - читаем микрофон
// - обновляем экран
// - пишем строку в Serial

void loop() {
  static unsigned long lastUpdate = 0;

  // Обновляемся не чаще одного раза в 200 мс.
  // Это примерно 5 раз в секунду.
  //
  // Для индикатора громкости достаточно.
  // Экран не дёргается как нервный ёжик.

  if (millis() - lastUpdate < 200) {
    return;
  }

  lastUpdate = millis();

  int percent = 0;
  int32_t level = 0;
  int32_t p2p = 0;

  bool ok = readMic(percent, level, p2p);

  if (!ok) {
    drawMicNoData();
    printBoth("mic=NO_DATA");
    return;
  }

  drawMicValues(percent, level, p2p);

  // Диагностическая строка для Serial Monitor.
  //
  // Пример:
  // mic=OK lvl=35% level=31234 p2p=125000

  printBoth(
    String("mic=OK ") +
    String("lvl=") + String(percent) + String("% ") +
    String("level=") + String(level) + String(" ") +
    String("p2p=") + String(p2p)
  );
}
