# ESP32-ASProject Project Log

## 2026-06-16

Created development branch for ESP32 acoustic/source-position emulator experiments.

### Current lab target

Build an educational sound-source geometry emulator on ESP32.

Initial hardware idea:

- ESP32-WROOM-32 board
- Arduino CLI on Lenovo ThinkPad T16
- USB-UART CH340
- Serial port: `/dev/ttyUSB0`
- Two potentiometers as virtual X/Y sound source controls
- Later: virtual microphone levels and geometry calculations

### Development environment confirmed

- OS: Ubuntu 24.04 on Lenovo ThinkPad T16
- Arduino IDE: 1.8.19 installed
- Arduino CLI: installed and working
- ESP32 core: installed
- ESP32 upload: successful
- Serial monitor: working

### Rules

- Use full scripts or full file contents.
- Do not patch files manually line by line.
- Prefer CLI workflow.
- Keep test scripts in `scripts/`.

## 2026-06-16: Three microphone point emulator

Added educational three-microphone point emulator.

### Model

Virtual microphones:

- MIC1: D34 / GPIO34
- MIC2: D35 / GPIO35
- MIC3: VP / GPIO36

Each potentiometer acts as a simulated microphone data source.

Output format:

- First line: frequency array for three microphones
- Second line: volume array for three microphones
- Then calculated sound point: X/Y
- Then ASCII coordinate grid

Coordinate model:

- X = MIC2 volume - MIC1 volume
- Y = MIC3 volume - average(MIC1 volume, MIC2 volume)

Audio output:

- GPIO25 / D25 through 1k resistor to headphone/speaker
- Main audible tone follows strongest virtual microphone
