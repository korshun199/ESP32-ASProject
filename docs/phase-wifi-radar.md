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
