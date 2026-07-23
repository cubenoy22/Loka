#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="${MAME_ENV_FILE:-$PROJECT_DIR/.env-mame}"

trim_whitespace() {
  local value="$1"
  value="${value#"${value%%[![:space:]]*}"}"
  value="${value%"${value##*[![:space:]]}"}"
  printf '%s' "$value"
}

expand_environment_value() {
  local value="$1"

  # Preserve common path syntax from the previously sourced env file without
  # evaluating arbitrary shell expressions.
  if [ "$value" = "~" ]; then
    value="$HOME"
  elif [[ "$value" == \~/* ]]; then
    value="$HOME/${value:2}"
  fi
  value="${value//\$\{HOME\}/$HOME}"
  value="${value//\$HOME/$HOME}"
  value="${value//\\ / }"
  printf '%s' "$value"
}

import_mame_environment() {
  local path="$1"
  local line name value

  while IFS= read -r line || [ -n "$line" ]; do
    line="${line%$'\r'}"
    line="$(trim_whitespace "$line")"
    if [ -z "$line" ] || [[ "$line" == \#* ]]; then
      continue
    fi
    if [[ ! "$line" =~ ^([A-Za-z_][A-Za-z0-9_]*)=(.*)$ ]]; then
      echo "Error: invalid environment line in $path" >&2
      exit 1
    fi

    name="${BASH_REMATCH[1]}"
    value="$(trim_whitespace "${BASH_REMATCH[2]}")"
    if [[ "$value" == \"*\" ]] || [[ "$value" == \'*\' ]]; then
      value="${value:1:${#value}-2}"
    fi
    value="$(expand_environment_value "$value")"
    export "$name=$value"
  done < "$path"
}

normalize_host_path() {
  local path="$1"
  if [[ "$path" =~ ^[A-Za-z]:\\ ]] && command -v wslpath >/dev/null 2>&1; then
    wslpath -u "$path"
  else
    printf '%s' "$path"
  fi
}

if [ -f "$ENV_FILE" ]; then
  import_mame_environment "$ENV_FILE"
fi

if [ $# -ne 1 ]; then
  echo "Usage: $0 <Retro68-MacBinary-file>" >&2
  exit 2
fi

MACBINARY_PATH="$1"
MAME_HDA="${MAME_HDA:-}"
MAME_HOMEPATH="${MAME_HOMEPATH:-$HOME/.mame}"
MAME_CONTROL_DIR="${MAME_CONTROL_DIR:-$MAME_HOMEPATH/loka}"
MAME_DEV_HDA="${MAME_DEV_HDA:-$PROJECT_DIR/build/mame-dev/LokaDev.hd}"
MAME_HDA="$(normalize_host_path "$MAME_HDA")"
MAME_HOMEPATH="$(normalize_host_path "$MAME_HOMEPATH")"
MAME_CONTROL_DIR="$(normalize_host_path "$MAME_CONTROL_DIR")"
MAME_DEV_HDA="$(normalize_host_path "$MAME_DEV_HDA")"

if [ ! -f "$MACBINARY_PATH" ]; then
  echo "Error: Retro68 MacBinary file not found: $MACBINARY_PATH" >&2
  exit 1
fi
if [ -z "$MAME_HDA" ] || [ ! -f "$MAME_HDA" ]; then
  echo "Error: MAME_HDA must point to the boot hard disk template" >&2
  exit 1
fi

find_retro68_tool() {
  local name="$1"
  local candidate

  if [ -n "${RETRO68_TOOLCHAIN_BIN:-}" ] \
    && [ -x "$RETRO68_TOOLCHAIN_BIN/$name" ]; then
    echo "$RETRO68_TOOLCHAIN_BIN/$name"
    return
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return
  fi
  if [ -n "${RETRO68_BUILD_DIR:-}" ] \
    && [ -x "$RETRO68_BUILD_DIR/toolchain/bin/$name" ]; then
    echo "$RETRO68_BUILD_DIR/toolchain/bin/$name"
    return
  fi
  for candidate in \
    "$HOME/Retro68-build/toolchain/bin/$name" \
    "$HOME/Documents/Projects/Retro68-build/toolchain/bin/$name"; do
    if [ -x "$candidate" ]; then
      echo "$candidate"
      return
    fi
  done
  echo "Error: Retro68 tool not found: $name" >&2
  exit 1
}

HFORMAT="$(find_retro68_tool hformat)"
HCOPY="$(find_retro68_tool hcopy)"
HUMOUNT="$(find_retro68_tool humount)"
HFS_HOME="$MAME_CONTROL_DIR/hfsutils"
TEMPORARY_DISK="$MAME_DEV_HDA.$$"

mkdir -p "$(dirname "$MAME_DEV_HDA")" "$HFS_HOME"
cp "$MAME_HDA" "$TEMPORARY_DISK"

cleanup() {
  HOME="$HFS_HOME" "$HUMOUNT" >/dev/null 2>&1 || true
  rm -f "$TEMPORARY_DISK"
}
trap cleanup EXIT

HOME="$HFS_HOME" "$HFORMAT" -l LokaDev "$TEMPORARY_DISK" 1
HOME="$HFS_HOME" "$HCOPY" -m "$MACBINARY_PATH" :
HOME="$HFS_HOME" "$HUMOUNT"
mv -f "$TEMPORARY_DISK" "$MAME_DEV_HDA"
trap - EXIT

echo "Prepared SCSI development disk: $MAME_DEV_HDA"
