# ESP32 real analog microphone selectable firmware

This firmware is for testing one real analog microphone module at a time.

Recommended microphone modules:

- MAX9814 analog electret microphone amplifier
- MAX4466 analog electret microphone amplifier
- SparkFun Electret Microphone Breakout or similar analog OUT module

Do not connect a bare electret capsule directly to ESP32 ADC.

Wiring:

VCC -> 3.3V
GND -> GND
OUT -> selected ESP32 analog input

Selectable pins in firmware:

D32 / GPIO32
D33 / GPIO33
D34 / GPIO34
D35 / GPIO35
VN  / GPIO39

Edit:

firmware/esp32_real_mic_selectable_sta/esp32_real_mic_selectable_sta.ino

Uncomment only one ACTIVE_MIC_PIN / ACTIVE_MIC_NAME pair.

Static IP:

http://192.168.20.77/
http://192.168.20.77/mobile
http://192.168.20.77/api/latest

Wi-Fi credentials are stored locally and ignored by Git:

firmware/esp32_real_mic_selectable_sta/wifi_config.h
