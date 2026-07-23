#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${1:-23946}"
POWERSHELL_SCRIPT="$(wslpath -w "$SCRIPT_DIR/mame-gdbstub-transport.ps1")"

exec powershell.exe -NoProfile -ExecutionPolicy Bypass \
  -File "$POWERSHELL_SCRIPT" -Port "$PORT"
