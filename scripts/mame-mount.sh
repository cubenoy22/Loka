#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
ENV_FILE="${MAME_ENV_FILE:-$PROJECT_DIR/.env-mame}"

if [ -f "$ENV_FILE" ]; then
  set -a
  # shellcheck source=/dev/null
  source "$ENV_FILE"
  set +a
fi

MAME_HOMEPATH="${MAME_HOMEPATH:-$HOME/.mame}"
MAME_CONTROL_DIR="${MAME_CONTROL_DIR:-$MAME_HOMEPATH/loka}"
REQUEST_PATH="$MAME_CONTROL_DIR/floppy.request"
RESPONSE_PATH="$MAME_CONTROL_DIR/floppy.response"

if [ $# -lt 1 ]; then
  echo "Usage: $0 <disk-image|--eject>" >&2
  exit 2
fi

mkdir -p "$MAME_CONTROL_DIR"
rm -f "$RESPONSE_PATH"

REQUEST_TEMP="$REQUEST_PATH.$$"
if [ "$1" = "--eject" ]; then
  printf 'eject\n' > "$REQUEST_TEMP"
else
  DSK_PATH="$1"
  if [ ! -f "$DSK_PATH" ]; then
    echo "Error: disk image not found: $DSK_PATH" >&2
    exit 1
  fi
  printf 'mount\n%s\n' "$DSK_PATH" > "$REQUEST_TEMP"
fi
mv -f "$REQUEST_TEMP" "$REQUEST_PATH"

for _ in $(seq 1 100); do
  if [ -f "$RESPONSE_PATH" ]; then
    STATUS="$(sed -n '1p' "$RESPONSE_PATH")"
    MESSAGE="$(sed -n '2p' "$RESPONSE_PATH")"
    rm -f "$RESPONSE_PATH"
    if [ "$STATUS" = "ok" ]; then
      echo "$MESSAGE"
      exit 0
    fi
    echo "Error: $MESSAGE" >&2
    exit 1
  fi
  sleep 0.1
done

echo "Error: MAME floppy service did not respond; start MAME first" >&2
exit 1
