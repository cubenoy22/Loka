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
  # WSL does not hand its environment to a Windows process unless the names
  # are listed in WSLENV, so the debugger switch would silently do nothing
  # here. Append rather than overwrite: the caller may be forwarding its own.
  for name in MAME_DEBUG MAME_DEBUG_PORT; do
    if [ -n "${!name:-}" ]; then
      case ":${WSLENV:-}:" in
        *":$name:"*) ;;
        *) WSLENV="${WSLENV:+$WSLENV:}$name" ;;
      esac
    fi
  done
  export WSLENV
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
MAME_BOOT_HDA="${MAME_BOOT_HDA:-$PROJECT_DIR/build/mame-run/Boot.hd}"

mkdir -p "$MAME_HOMEPATH" "$MAME_CONTROL_DIR"

# MAME writes back to whatever it boots, so never hand it MAME_HDA itself: that
# image is the Classic rail's template, and the pixels the goldens are made of
# live inside it. Boot a copy under build/ instead, the same shape
# mame-debug.sh uses. The copy persists so an interactive session keeps its
# state; wiping build/ resets it to the template.
if [ -n "$MAME_HDA" ]; then
  if [ ! -f "$MAME_HDA" ]; then
    echo "boot hard disk template not found: $MAME_HDA" >&2
    exit 1
  fi
  mkdir -p "$(dirname "$MAME_BOOT_HDA")"
  # An alias defeats the whole point: the existence check would pass and -hard1
  # would name the template after all. -ef compares device and inode, so a
  # symlink or hard link is caught as well as the same path spelled twice.
  if [ -e "$MAME_BOOT_HDA" ] && [ "$MAME_BOOT_HDA" -ef "$MAME_HDA" ]; then
    echo "MAME_BOOT_HDA resolves to the boot template itself: $MAME_BOOT_HDA" >&2
    exit 1
  fi
  # Copy through a temporary and rename, so an interrupted copy cannot leave a
  # truncated image behind: this run would exit, but every later run would find
  # a regular file, skip the copy, and boot the corrupt one.
  if [ ! -f "$MAME_BOOT_HDA" ]; then
    if ! cp -f "$MAME_HDA" "$MAME_BOOT_HDA.partial" ||
       ! mv -f "$MAME_BOOT_HDA.partial" "$MAME_BOOT_HDA"; then
      rm -f "$MAME_BOOT_HDA.partial"
      echo "could not copy the boot hard disk template to $MAME_BOOT_HDA" >&2
      exit 1
    fi
  fi
fi
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
  MAME_ARGS+=(-hard1 "$MAME_BOOT_HDA")
fi

MAME_ARGS+=(-scsi:5 harddisk)
if [ -f "$MAME_DEV_HDA" ]; then
  MAME_ARGS+=(-hard2 "$MAME_DEV_HDA")
fi

MAME_ARGS+=(
  -autoboot_script "$SCRIPT_DIR/mame-floppy-service.lua"
)

# MAME_DEBUG=1 exposes the CPU to gdb: MAME halts at reset and listens on
# MAME_DEBUG_PORT until a gdb (e.g. gdb-multiarch via the "Attach (MAME 68K
# gdbstub)" VS Code configuration) connects and continues.
if [ -n "${MAME_DEBUG:-}" ]; then
  MAME_ARGS+=(-debug -debugger gdbstub -debugger_port "${MAME_DEBUG_PORT:-23946}")
fi

exec "$MAME_EXECUTABLE" "${MAME_ARGS[@]}"
