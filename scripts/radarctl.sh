#!/usr/bin/env bash
set -uo pipefail

PROJECT_DIR="/home/work/ESP32-ASProject"
RUN_DIR="${PROJECT_DIR}/.run"
LOG_DIR="${PROJECT_DIR}/logs"
SERVER_PID_FILE="${RUN_DIR}/radar_server.pid"
SERVER_LOG="${LOG_DIR}/radar_server.log"

DEFAULT_PORT="/dev/ttyUSB0"
DEFAULT_BAUD="115200"
SERVER_URL="http://127.0.0.1:8088"
CONTROL_URL="${SERVER_URL}/control.html?v=47"

RESET="\033[0m"
BOLD="\033[1m"
DIM="\033[2m"
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
MAGENTA="\033[35m"
CYAN="\033[36m"

mkdir -p "${RUN_DIR}" "${LOG_DIR}"

c() {
  printf "%b%s%b\n" "$1" "$2" "${RESET}"
}

title() {
  clear
  c "${BOLD}${CYAN}" "ESP32 RADAR — ПОЛЕВОЙ ПУЛЬТ"
  c "${DIM}" "Проект: ${PROJECT_DIR}"
  echo
}

ok() {
  c "${GREEN}" "✅ $1"
}

warn() {
  c "${YELLOW}" "⚠️  $1"
}

err() {
  c "${RED}" "❌ $1"
}

info() {
  c "${CYAN}" "$1"
}

pause() {
  echo
  read -r -p "Нажми Enter для продолжения..." _
}

go_project() {
  cd "${PROJECT_DIR}" || {
    err "Не могу перейти в проект: ${PROJECT_DIR}"
    exit 1
  }
}

is_server_running() {
  [[ -f "${SERVER_PID_FILE}" ]] || return 1

  local pid
  pid="$(cat "${SERVER_PID_FILE}" 2>/dev/null || true)"

  [[ -n "${pid}" ]] || return 1
  kill -0 "${pid}" 2>/dev/null
}

server_pid() {
  cat "${SERVER_PID_FILE}" 2>/dev/null || true
}

start_server() {
  title
  go_project

  if is_server_running; then
    ok "Сервер уже запущен. PID: $(server_pid)"
    info "URL: ${CONTROL_URL}"
    pause
    return
  fi

  info "Стартую сервер радара в фоне..."
  info "Лог: ${SERVER_LOG}"
  echo

  nohup python3 web/radar_virtual_mics/server.py > "${SERVER_LOG}" 2>&1 &
  local pid=$!
  echo "${pid}" > "${SERVER_PID_FILE}"

  sleep 1

  if is_server_running; then
    ok "Сервер запущен. PID: ${pid}"
    info "URL: ${CONTROL_URL}"
  else
    err "Сервер не стартовал. Последние строки лога:"
    tail -40 "${SERVER_LOG}" 2>/dev/null || true
  fi

  pause
}

stop_server() {
  title

  if ! is_server_running; then
    warn "Сервер не запущен."
    rm -f "${SERVER_PID_FILE}"
    pause
    return
  fi

  local pid
  pid="$(server_pid)"

  info "Останавливаю сервер PID ${pid}..."
  kill "${pid}" 2>/dev/null || true
  sleep 1

  if kill -0 "${pid}" 2>/dev/null; then
    warn "Мягко не остановился, добиваю..."
    kill -9 "${pid}" 2>/dev/null || true
  fi

  rm -f "${SERVER_PID_FILE}"
  ok "Сервер остановлен."
  pause
}

show_status() {
  title
  go_project

  echo -e "${BOLD}Git:${RESET}"
  git branch --show-current 2>/dev/null || true
  git status --short 2>/dev/null || true

  echo
  echo -e "${BOLD}Сервер:${RESET}"
  if is_server_running; then
    ok "Запущен. PID: $(server_pid)"
  else
    warn "Не запущен."
  fi

  echo
  echo -e "${BOLD}Порты:${RESET}"
  ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || warn "Serial-порты не найдены."

  echo
  echo -e "${BOLD}Arduino CLI платы:${RESET}"
  arduino-cli board list 2>/dev/null || warn "arduino-cli board list не сработал."

  echo
  echo -e "${BOLD}Последние строки лога сервера:${RESET}"
  tail -20 "${SERVER_LOG}" 2>/dev/null || warn "Лога сервера пока нет."

  pause
}

open_control() {
  title

  info "Открываю web control:"
  info "${CONTROL_URL}"
  echo

  if command -v xdg-open >/dev/null 2>&1; then
    xdg-open "${CONTROL_URL}" >/dev/null 2>&1 &
    ok "Команда открытия отправлена в систему."
  else
    warn "xdg-open не найден. Открой вручную:"
    echo "${CONTROL_URL}"
  fi

  pause
}

mic_dashboard() {
  title
  go_project

  echo -e "${YELLOW}Важно:${RESET} это занимает порт ${DEFAULT_PORT}."
  echo "Не запускай одновременно raw monitor, bridge или arduino-cli monitor."
  echo

  read -r -p "Порт [${DEFAULT_PORT}]: " port
  port="${port:-$DEFAULT_PORT}"

  info "Запускаю табло микрофонов на ${port}..."
  sleep 1

  ./scripts/radar_mic_dashboard.py --port "${port}" --baud "${DEFAULT_BAUD}"
}

raw_serial_monitor() {
  title
  go_project

  echo -e "${YELLOW}Важно:${RESET} это сырой serial monitor. Для диагностики удобнее пункт 5."
  echo

  read -r -p "Порт [${DEFAULT_PORT}]: " port
  port="${port:-$DEFAULT_PORT}"

  info "Запускаю сырой монитор ${port} ${DEFAULT_BAUD}..."
  echo "Выход обычно Ctrl+C."
  echo

  arduino-cli monitor -p "${port}" -c "baudrate=${DEFAULT_BAUD}"
}

compile_verbose() {
  title
  go_project

  if [[ -x "./scripts/compile_single_mic_verbose.sh" ]]; then
    ./scripts/compile_single_mic_verbose.sh
  else
    warn "Цветной скрипт не найден, запускаю обычную компиляцию."
    arduino-cli compile --fqbn esp32:esp32:esp32 firmware/esp32_single_mic_serial
  fi

  pause
}

upload_firmware() {
  title
  go_project

  read -r -p "Порт [${DEFAULT_PORT}]: " port
  port="${port:-$DEFAULT_PORT}"

  info "Прошивка ESP32 через ${port}..."
  echo

  ./scripts/upload_single_mic_serial.sh "${port}"

  pause
}

show_ports() {
  title

  echo -e "${BOLD}Serial-порты:${RESET}"
  ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || warn "Serial-порты не найдены."

  echo
  echo -e "${BOLD}Arduino CLI board list:${RESET}"
  arduino-cli board list 2>/dev/null || warn "arduino-cli board list не сработал."

  echo
  echo -e "${BOLD}Последние USB-события ядра:${RESET}"
  dmesg | tail -30 2>/dev/null || true

  pause
}

show_description() {
  title

  cat <<TXT
Назначение пульта:

  Это полевое меню для работы с ESP32 Radar,
  когда ноутбук на коленке, солнце в экран, провода в руках,
  а мозг уже пытается вспомнить, как включается чайник.

Главное правило:

  VS Code  — смотреть код, подсказки, структура.
  Терминал — сборка, прошивка, сервер, мониторинг, Git.

Основные режимы:

  Сервер:
    запускает web/radar_virtual_mics/server.py в фоне.
    Лог: ${SERVER_LOG}
    URL: ${CONTROL_URL}

  Табло микрофонов:
    scripts/radar_mic_dashboard.py
    показывает 5 каналов, громкость, raw/min/max/p2p/freq.

  Сырой Serial:
    arduino-cli monitor
    нужен только если надо увидеть настоящую кашу из порта.

  Цветная компиляция:
    scripts/compile_single_mic_verbose.sh

  Прошивка:
    scripts/upload_single_mic_serial.sh

Важно про порт:

  /dev/ttyUSB0 нельзя открыть двумя программами сразу.
  Если открыт dashboard, raw monitor или bridge не запустится нормально.

TXT

  pause
}

show_logs() {
  title

  echo -e "${BOLD}Лог сервера:${RESET} ${SERVER_LOG}"
  echo

  if [[ -f "${SERVER_LOG}" ]]; then
    tail -80 "${SERVER_LOG}"
  else
    warn "Лог сервера пока не найден."
  fi

  pause
}

git_status() {
  title
  go_project

  git status --short
  echo
  git log --oneline -5

  pause
}

menu() {
  while true; do
    title

    if is_server_running; then
      echo -e "Сервер: ${GREEN}ЗАПУЩЕН${RESET} PID $(server_pid)"
    else
      echo -e "Сервер: ${YELLOW}НЕ ЗАПУЩЕН${RESET}"
    fi

    echo -e "Порт по умолчанию: ${BOLD}${DEFAULT_PORT}${RESET}"
    echo

    echo "  1) Старт сервера"
    echo "  2) Стоп сервера"
    echo "  3) Статус проекта и портов"
    echo "  4) Открыть web control"
    echo "  5) Табло микрофонов"
    echo "  6) Сырой Serial monitor"
    echo "  7) Цветная компиляция"
    echo "  8) Прошить ESP32"
    echo "  9) Порты и USB-события"
    echo " 10) Лог сервера"
    echo " 11) Git status"
    echo " 12) Описание"
    echo
    echo "  0) Выход"
    echo

    read -r -p "Выбор: " choice

    case "${choice}" in
      1) start_server ;;
      2) stop_server ;;
      3) show_status ;;
      4) open_control ;;
      5) mic_dashboard ;;
      6) raw_serial_monitor ;;
      7) compile_verbose ;;
      8) upload_firmware ;;
      9) show_ports ;;
      10) show_logs ;;
      11) git_status ;;
      12) show_description ;;
      0|q|Q|й|Й)
        echo
        ok "Выход."
        exit 0
        ;;
      *)
        warn "Неизвестный пункт: ${choice}"
        sleep 1
        ;;
    esac
  done
}

case "${1:-menu}" in
  menu) menu ;;
  start-server) start_server ;;
  stop-server) stop_server ;;
  status) show_status ;;
  control) open_control ;;
  dashboard) mic_dashboard ;;
  monitor) raw_serial_monitor ;;
  compile) compile_verbose ;;
  upload) upload_firmware ;;
  ports) show_ports ;;
  logs) show_logs ;;
  git) git_status ;;
  help|description) show_description ;;
  *)
    err "Неизвестная команда: $1"
    echo "Доступно: menu, start-server, stop-server, status, control, dashboard, monitor, compile, upload, ports, logs, git, help"
    exit 1
    ;;
esac
