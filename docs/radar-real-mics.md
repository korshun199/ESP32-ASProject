# ESP32 Radar: real analog microphones

Final current firmware:

firmware/esp32_radar_real_mics/esp32_radar_real_mics.ino

Purpose:

- ESP32 connects to local Wi-Fi as a station.
- Static IP: 192.168.20.77
- Browser UI shows all enabled microphone channels.
- API returns active microphone arrays.
- User enables channels by commenting/uncommenting USE_MIC lines in the firmware.

Recommended microphone modules:

- MAX9814 analog electret microphone amplifier
- MAX4466 analog electret microphone amplifier
- SparkFun Electret Microphone Breakout or similar analog OUT module

Do not connect a bare electret capsule directly to ESP32 ADC.

Wiring:

VCC -> 3.3V
GND -> GND
OUT -> selected ESP32 analog input

Available ADC1 pins:

MIC1 = D32 / GPIO32
MIC2 = D33 / GPIO33
MIC3 = D34 / GPIO34
MIC4 = D35 / GPIO35
MIC5 = VN  / GPIO39

Edit this block in firmware:

#define USE_MIC1_D32
#define USE_MIC2_D33
#define USE_MIC3_D34
#define USE_MIC4_D35
#define USE_MIC5_VN

Comment out channels without physically connected microphones.

Local Wi-Fi credentials are stored here and ignored by Git:

firmware/esp32_radar_real_mics/wifi_config.h

Set Wi-Fi:

./scripts/set_radar_wifi.sh

Flash:

./scripts/upload_radar_mics.sh /dev/ttyUSB0

Open:

http://192.168.20.77/
http://192.168.20.77/mobile
http://192.168.20.77/api/latest
