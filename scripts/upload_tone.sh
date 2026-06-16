#!/usr/bin/env bash
set -e

FREQ="${1:-440}"

PROJECT_DIR="/home/work/ESP32-ASProject"
SKETCH_DIR="$PROJECT_DIR/build/tone"
SKETCH_FILE="$SKETCH_DIR/tone.ino"

PORT="/dev/ttyUSB0"
FQBN="esp32:esp32:esp32"

mkdir -p "$SKETCH_DIR"

cat > "$SKETCH_FILE" <<EOF
const int AUDIO_PIN = 25;

void setup() {
  Serial.begin(115200);

  ledcAttachChannel(
    AUDIO_PIN,
    ${FREQ},
    8,
    0
  );

  ledcWrite(AUDIO_PIN, 128);

  ledcWriteTone(AUDIO_PIN, ${FREQ});

  Serial.println();
  Serial.print("Tone frequency: ");
  Serial.println(${FREQ});
}

void loop() {
  delay(1000);
}
EOF

echo "===== COMPILE ====="
arduino-cli compile \
  --fqbn "$FQBN" \
  "$SKETCH_DIR"

echo
echo "===== UPLOAD ====="
arduino-cli upload \
  -p "$PORT" \
  --fqbn "$FQBN" \
  "$SKETCH_DIR"

echo
echo "===== DONE ====="
echo "Frequency: ${FREQ} Hz"