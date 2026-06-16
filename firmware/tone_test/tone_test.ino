/*
  ESP32-ASProject
  Tone test 100..800 Hz

  Output:
    GPIO25 -> resistor 1k..2.2k -> headphone/small speaker
    GND    -> second headphone/speaker contact

  WARNING:
    Do not connect headphones directly without resistor.
*/

const int AUDIO_PIN = 25;
const int PWM_CHANNEL = 0;
const int PWM_RESOLUTION = 8;

int freq = 100;
int stepHz = 10;

void setup() {
  Serial.begin(115200);
  delay(500);

  ledcAttachChannel(AUDIO_PIN, freq, PWM_RESOLUTION, PWM_CHANNEL);
  ledcWrite(AUDIO_PIN, 128);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 tone generator started");
  Serial.println("GPIO25 -> resistor -> headphone");
  Serial.println("Frequency sweep: 100..800 Hz");
  Serial.println("================================");
}

void loop() {
  ledcWriteTone(AUDIO_PIN, freq);

  Serial.print("Tone Hz: ");
  Serial.println(freq);

  freq += stepHz;

  if (freq >= 800) {
    freq = 800;
    stepHz = -10;
  }

  if (freq <= 100) {
    freq = 100;
    stepHz = 10;
  }

  delay(120);
}
