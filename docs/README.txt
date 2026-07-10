Проект Радар
Финал первой серии

Рабочие файлы проекта:

1. Прошивка
firmware/radar/radar.ino

2. Запуск прошивки
scripts/run.sh

3. Монитор порта
scripts/monitor.sh

Плата:
ESP32-S3 N16R8

TFT экран:
Драйвер: Adafruit_ST7789
Инициализация: tft.init(240, 320)
Поворот: tft.setRotation(0)

Пины TFT:
CS   -> GPIO10
DC   -> GPIO9
RST  -> GPIO8
MOSI -> GPIO11
SCLK -> GPIO12
VCC  -> 3V3
GND  -> GND
LED  -> 3V3

Микрофон ICS-43434:
VDD/VCC  -> 3V3
GND      -> GND
SCK/BCLK -> GPIO4
WS/LRCLK -> GPIO5
SD/DOUT  -> GPIO6
L/R      -> GND

Что работает:
TFT ST7789 работает на полном экране.
ICS-43434 работает по I2S.
На экране отображаются SIMONG RADAR, RADAR, SYS, MIC OK, LVL и P2P.

Главное правило второй серии:
Продолжать только от firmware/radar/radar.ino.
Не возвращаться к ILI9341.
Не тащить старые тестовые скетчи.
