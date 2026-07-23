#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
PORT="${1:-23946}"
POWERSHELL_SCRIPT="$(wslpath -w "$SCRIPT_DIR/mame-gdbstub-windows.ps1")"
mkdir -p "$PROJECT_DIR/build/mame-dev"
OWNER_PID_FILE="$(mktemp "$PROJECT_DIR/build/mame-dev/mame-gdbstub.XXXXXX.pid")"
WINDOWS_OWNER_PID_FILE="$(wslpath -w "$OWNER_PID_FILE")"
POWERSHELL_BRIDGE_PID=""

cleanup() {
  local launcher_pid=""

  trap - EXIT HUP INT TERM
  if [ -s "$OWNER_PID_FILE" ]; then
    IFS= read -r launcher_pid < "$OWNER_PID_FILE" || true
    if [[ "$launcher_pid" =~ ^[0-9]+$ ]]; then
      taskkill.exe /PID "$launcher_pid" /T /F >/dev/null 2>&1 || true
    fi
  fi
  if [ -n "$POWERSHELL_BRIDGE_PID" ]; then
    kill "$POWERSHELL_BRIDGE_PID" >/dev/null 2>&1 || true
  fi
  rm -f -- "$OWNER_PID_FILE"
}

trap cleanup EXIT HUP INT TERM
powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File "$POWERSHELL_SCRIPT" -Port "$PORT" \
  -OwnerPidFile "$WINDOWS_OWNER_PID_FILE" &
POWERSHELL_BRIDGE_PID="$!"
wait "$POWERSHELL_BRIDGE_PID"
