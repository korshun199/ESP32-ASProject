#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "FS.h"

#include <LittleFS.h>

char vps_ip[40] = "0.0.0.0"; // Сюда сохранится IP из настроек
bool shouldSaveConfig = false;

WiFiClient client;
bool wasConnected = false;

// Функция для сохранения настроек
void saveConfigCallback() {
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  
  // 1. Инициализация файловой системы для хранения IP
  if (!LittleFS.begin(true)) {
    Serial.println("Ошибка файловой системы");
  }

  // Читаем старый IP из памяти, если он там есть
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      StaticJsonDocument<200> json;
      deserializeJson(json, configFile);
      strcpy(vps_ip, json["vps_ip"]);
      configFile.close();
    }
  }

  // 2. Настройка WiFiManager с доп. полем
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  // Создаем поле ввода для IP в интерфейсе телефона
  WiFiManagerParameter custom_vps_ip("server", "IP адрес VPS", vps_ip, 40);
  wm.addParameter(&custom_vps_ip);

  if (!wm.autoConnect("ESP32_CONFIG")) {
    delay(3000);
    ESP.restart();
  }

  // 3. Сохраняем новый IP, если пользователь его ввел
  strcpy(vps_ip, custom_vps_ip.getValue());
  if (shouldSaveConfig) {
    File configFile = LittleFS.open("/config.json", "w");
    StaticJsonDocument<200> json;
    json["vps_ip"] = vps_ip;
    serializeJson(json, configFile);
    configFile.close();
  }

  Serial.println("Подключено! Целевой сервер: " + String(vps_ip));
}

void loop() {
  if (!client.connected()) {
    if (wasConnected) { Serial.println("Lost Connection"); wasConnected = false; }
    
    // Пытаемся подключиться к IP, который ввели в WiFiManager
    if (client.connect(vps_ip, 8000)) {
      Serial.println("Connected to VPS: " + String(vps_ip));
      wasConnected = true;
    } else {
      delay(5000);
      return;
    }
  }

  while (client.available()) {
    String command = client.readStringUntil('\n');
    command.trim();
    if (command.length() > 0) {
      Serial.println("ПОЛУЧЕНО: " + command);
      client.println("ACK: OK");
    }
  }
  delay(10);
}
