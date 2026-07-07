#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="${PROJECT_DIR}/firmware/esp32_single_mic_serial"
BUILD_DIR="${PROJECT_DIR}/build/vscode-intellisense"
FQBN="esp32:esp32:esp32"

echo "📡 Обновляю базу компиляции для VS Code..."
echo "📁 Проект: ${PROJECT_DIR}"
echo "🧠 Скетч:  ${SKETCH_DIR}"
echo "🔧 Плата:  ${FQBN}"
echo

mkdir -p "${BUILD_DIR}"

arduino-cli compile \
  --fqbn "${FQBN}" \
  --build-path "${BUILD_DIR}" \
  --only-compilation-database \
  "${SKETCH_DIR}"

if [[ -f "${BUILD_DIR}/compile_commands.json" ]]; then
  cp "${BUILD_DIR}/compile_commands.json" "${PROJECT_DIR}/compile_commands.json"
  echo
  echo "✅ Готово:"
  echo "   ${PROJECT_DIR}/compile_commands.json"
else
  echo "❌ Не найден compile_commands.json в ${BUILD_DIR}" >&2
  exit 1
fi
