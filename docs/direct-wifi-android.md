# ESP32 Direct Wi-Fi Android prototype

Новая ветка для автономного режима без Python/FastAPI сервера.

Схема:

ESP32 -> Wi-Fi Access Point -> Android browser

Wi-Fi:

SSID: ESP32-RADAR
PASS: 12345678
URL : http://192.168.4.1

API:

GET /api/latest

Поля:

freq[3], vol[3], x, y, frame, mode, ssid, ip, status

Пины:

MIC1 = D34 / GPIO34
MIC2 = D35 / GPIO35
MIC3 = VP / GPIO36
SOUND = D25 / GPIO25

Это первый минимальный прототип. Интерфейс пока простой, без красоты.
Главная цель: проверить прямой обмен ESP32 -> Android без ноутбука.
