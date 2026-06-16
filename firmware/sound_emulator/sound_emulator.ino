const int AUDIO_PIN = 25;
const int ADC_PIN = 34;

int lastFreq = -1;

void setup() {
  Serial.begin(115200);
  delay(500);

  ledcAttachChannel(AUDIO_PIN, 440, 8, 0);
  ledcWrite(AUDIO_PIN, 128);

  Serial.println("ESP32 Sound Emulator");
  Serial.println("GPIO25 = sound out");
  Serial.println("GPIO34 = potentiometer input");
}

void loop() {
  int adc = analogRead(ADC_PIN);
  int freq = map(adc, 0, 4095, 100, 800);

  if (freq != lastFreq) {
    ledcWriteTone(AUDIO_PIN, freq);

    Serial.print("ADC=");
    Serial.print(adc);
    Serial.print(" FREQ=");
    Serial.print(freq);
    Serial.println(" Hz");

    lastFreq = freq;
  }

  delay(20);
}
