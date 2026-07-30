#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
LRPC=${LOKA_LRPC:-"$PROJECT_DIR/build/host/lrpc/lrpc"}

python3 "$SCRIPT_DIR/assets/generate_picts.py"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest.txt" \
  -o "$SCRIPT_DIR/ASSETS.LRP" \
  --stamp "$SCRIPT_DIR/ASSETS.stamp"
