#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# VS Code connected to WSL sees Linux task definitions. Delegate to the
# Windows launcher so the existing MAME tasks still use the host installation.
if [ -n "${WSL_INTEROP:-}" ] && command -v powershell.exe >/dev/null 2>&1; then
  POWERSHELL_SCRIPT="$(wslpath -w "$SCRIPT_DIR/mame-run.ps1")"
  POWERSHELL_ARGS=(-NoProfile -ExecutionPolicy Bypass -File "$POWERSHELL_SCRIPT")
  if [ -n "${MAME_ENV_FILE:-}" ]; then
    WINDOWS_ENV_FILE="$MAME_ENV_FILE"
    if [[ ! "$WINDOWS_ENV_FILE" =~ ^[A-Za-z]:\\ ]]; then
      WINDOWS_ENV_FILE="$(wslpath -w "$WINDOWS_ENV_FILE")"
    fi
    POWERSHELL_ARGS+=(-EnvironmentFile "$WINDOWS_ENV_FILE")
  fi
  exec powershell.exe "${POWERSHELL_ARGS[@]}"
fi

# Load environment from .env-mame (project root or override via MAME_ENV_FILE)
ENV_FILE="${MAME_ENV_FILE:-$PROJECT_DIR/.env-mame}"
if [ -f "$ENV_FILE" ]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

MAME_MACHINE="${MAME_MACHINE:-maciici}"
MAME_RAMSIZE="${MAME_RAMSIZE:-8M}"
MAME_HDA="${MAME_HDA:-}"
MAME_ROMPATH="${MAME_ROMPATH:-}"
MAME_HOMEPATH="${MAME_HOMEPATH:-$HOME/.mame}"
MAME_EXECUTABLE="${MAME_EXECUTABLE:-mame}"
MAME_CONTROL_DIR="${MAME_CONTROL_DIR:-$MAME_HOMEPATH/loka}"
MAME_DEV_HDA="${MAME_DEV_HDA:-$PROJECT_DIR/build/mame-dev/LokaDev.hd}"

mkdir -p "$MAME_HOMEPATH" "$MAME_CONTROL_DIR"
export LOKA_MAME_FLOPPY_REQUEST="$MAME_CONTROL_DIR/floppy.request"
export LOKA_MAME_FLOPPY_RESPONSE="$MAME_CONTROL_DIR/floppy.response"

# Keep launcher policy aligned with mame-run.ps1; only shell mechanics differ.
MAME_ARGS=(
  "$MAME_MACHINE"
  -ramsize "$MAME_RAMSIZE"
  -homepath "$MAME_HOMEPATH"
  -cfg_directory "$MAME_HOMEPATH/cfg"
  -nvram_directory "$MAME_HOMEPATH/nvram"
  -snapshot_directory "$MAME_HOMEPATH/snap"
  -diff_directory "$MAME_HOMEPATH/diff"
)

if [ -n "$MAME_ROMPATH" ]; then
  MAME_ARGS+=(-rompath "$MAME_ROMPATH")
fi

if [ -n "$MAME_HDA" ]; then
  MAME_ARGS+=(-hard1 "$MAME_HDA")
fi

MAME_ARGS+=(-scsi:5 harddisk)
if [ -f "$MAME_DEV_HDA" ]; then
  MAME_ARGS+=(-hard2 "$MAME_DEV_HDA")
fi

MAME_ARGS+=(
  -autoboot_script "$SCRIPT_DIR/mame-floppy-service.lua"
)

exec "$MAME_EXECUTABLE" "${MAME_ARGS[@]}"
