# ESP32 Direct Wi-Fi STA 5 microphone prototype

This firmware connects ESP32 to the normal Wi-Fi network instead of creating ESP32-RADAR AP.

Pins:

MIC1 = D32 / GPIO32
MIC2 = D33 / GPIO33
MIC3 = D34 / GPIO34
MIC4 = D35 / GPIO35
MIC5 = VN  / GPIO39

Avoid ADC2 pins while Wi-Fi is enabled.

Local secret file, not committed:

firmware/esp32_direct_wifi_5mic_sta/wifi_config.h

Example:

#pragma once
#define WIFI_SSID "your_wifi_name"
#define WIFI_PASS "your_wifi_password"

After flashing, check Serial Monitor for IP address.

Open in Android browser:

http://ESP32_IP/
http://ESP32_IP/mobile

API:

GET /api/latest
