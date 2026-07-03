# ESP32 Radar

Final working project for ESP32 acoustic radar with real analog microphone modules.

Current firmware:

firmware/esp32_radar_real_mics/esp32_radar_real_mics.ino

Wi-Fi setup:

./scripts/set_radar_wifi.sh

Flash ESP32:

./scripts/upload_radar_mics.sh /dev/ttyUSB0

Open interface:

http://192.168.4.77/
http://192.168.4.77/mobile
http://192.168.4.77/api/latest

Microphone pins:

MIC1 -> D32 / GPIO32
MIC2 -> D33 / GPIO33
MIC3 -> D34 / GPIO34
MIC4 -> D35 / GPIO35
MIC5 -> VN  / GPIO39

Use analog microphone modules with amplifier, for example MAX9814 or MAX4466.

Do not connect a bare electret microphone capsule directly to ESP32 ADC.
