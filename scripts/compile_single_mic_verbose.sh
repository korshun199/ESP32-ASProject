#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="${PROJECT_DIR}/firmware/esp32_single_mic_serial"
FQBN="${FQBN:-esp32:esp32:esp32}"

RESET="\033[0m"
BOLD="\033[1m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
CYAN="\033[36m"
MAGENTA="\033[35m"
DIM="\033[2m"

print_title() {
  printf "${BOLD}${CYAN}=== %s ===${RESET}\n" "$1"
}

print_ok() {
  printf "${GREEN}✅ %s${RESET}\n" "$1"
}

print_error() {
  printf "${RED}❌ %s${RESET}\n" "$1"
}

print_info() {
  printf "${CYAN}%s${RESET}\n" "$1"
}

print_title "Цветная компиляция ESP32 single mic"
print_info "Проект: ${PROJECT_DIR}"
print_info "Скетч:  ${SKETCH_DIR}"
print_info "Плата:  ${FQBN}"
echo

cd "${PROJECT_DIR}"

set +e

arduino-cli compile \
  --verbose \
  --fqbn "${FQBN}" \
  "${SKETCH_DIR}" 2>&1 | awk '
BEGIN {
  red="\033[31m"
  green="\033[32m"
  yellow="\033[33m"
  cyan="\033[36m"
  magenta="\033[35m"
  dim="\033[2m"
  reset="\033[0m"
}

/error:|Ошибка|fatal error|exit status/ {
  print red $0 reset
  next
}

/warning:|предупреждение|Warning/ {
  print yellow $0 reset
  next
}

/Скетч использует|Глобальные переменные|Hash of data verified|Hard resetting|Готово|Done/ {
  print green $0 reset
  next
}

/Compiling|Linking|Archiving|Building|Generating|Using board|Использована платформа|Плата|Sketch/ {
  print cyan $0 reset
  next
}

/^$/ {
  print
  next
}

{
  print dim $0 reset
}
'

compile_status=${PIPESTATUS[0]}
set -e

echo

if [[ "${compile_status}" -eq 0 ]]; then
  print_ok "Компиляция успешно завершена"
else
  print_error "Компиляция завершилась с ошибкой: ${compile_status}"
fi

exit "${compile_status}"
