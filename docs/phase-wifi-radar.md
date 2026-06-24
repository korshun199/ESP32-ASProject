# ESP32 WiFi Radar

Цель:
ESP32 -> WiFi -> Android Browser

Этап 1:
- ESP32 поднимает AP ESP32-RADAR
- веб-сервер на 192.168.4.1
- отдаёт страницу index.html

Этап 2:
- WebSocket
- передача freq[3], vol[3], x, y

Этап 3:
- визуализация точки источника на Canvas

## 2026-06-23: MAX9814 loudness graph MVP

Зафиксирована рабочая точка ESP32 Wi-Fi Radar:

- плата ESP32-D0WD-V3 / ESP32-WROOM-32;
- порт прошивки: /dev/ttyUSB0;
- ESP32 поднимает собственную Wi-Fi сеть ESP32-RADAR;
- веб-интерфейс доступен по адресу http://192.168.4.1;
- API доступен по адресу http://192.168.4.1/api/latest;
- подключён аналоговый микрофонный модуль MAX9814;
- MAX9814 подключается:
  - G -> GND ESP32
  - 5V+ -> 3.3V ESP32
  - Dot -> GPIO34
  - AR -> не подключать
- прошивка читает окно ADC-измерений;
- вычисляются raw, min, max, vol;
- в веб-интерфейсе добавлены:
  - индикатор текущей громкости;
  - бегущий график громкости во времени;
  - вывод сырых JSON-данных.

Текущая цель:
добиться устойчивой реакции графика на речь и хлопки, затем перейти к улучшению обработки сигнала.

## 2026-06-24: Radar + ESP device probe checkpoint

Зафиксирована рабочая точка:

### ESP32 Wi-Fi Radar
- Ветка: dev/esp32-direct-wifi-android
- Плата: ESP32-D0WD-V3 / ESP32-WROOM-32
- Порт: /dev/ttyUSB0
- Flash: 4MB
- Wi-Fi AP: ESP32-RADAR
- URL: http://192.168.4.1
- API: http://192.168.4.1/api/latest
- Микрофон: MAX9814
- Подключение MAX9814:
  - G -> GND
  - 5V+ -> 3.3V
  - Dot -> GPIO34
  - AR -> не подключать
- Прошивка показывает:
  - raw
  - min
  - max
  - vol
  - текущую громкость
  - бегущий график громкости

### ESP Device Probe
Добавлен универсальный скрипт:

scripts/esp_device_probe.sh

Назначение:
- определить неизвестную ESP-плату;
- получить chip_id, MAC, flash_id;
- сохранить полный дамп flash;
- сохранить SHA256;
- считать partitions.bin;
- создать partitions.csv с русской шапкой;
- создать partitions_raw.csv для скриптов;
- создать flash_map.txt;
- создать HTML-отчёт;
- извлечь app/factory/OTA бинарники из flash в extracted/;
- сохранить restore_commands.txt.

Идея:
воткнул неизвестную железку, запустил скрипт, получил паспорт устройства.
