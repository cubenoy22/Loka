#!/usr/bin/env sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_DIR=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
LRPC=${LOKA_LRPC:-"$PROJECT_DIR/build/host/lrpc/lrpc"}
BAKE_DIR=$(mktemp -d "${TMPDIR:-/tmp}/loka-scrapbook-bake.XXXXXX")
trap 'rm -rf "$BAKE_DIR"' EXIT HUP INT TERM

python3 "$SCRIPT_DIR/assets/generate_picts.py"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest.txt" \
  -o "$BAKE_DIR/ASSETS-classic.LRP" \
  --stamp "$BAKE_DIR/ASSETS-classic.stamp" \
  --require "$SCRIPT_DIR/assets/scrapbook.pkgreq"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest-modern.txt" \
  -o "$BAKE_DIR/ASSETS-modern.LRP" \
  --stamp "$BAKE_DIR/ASSETS-modern.stamp" \
  --require "$SCRIPT_DIR/assets/scrapbook.pkgreq"

if ! cmp -s "$BAKE_DIR/ASSETS-classic.stamp" "$BAKE_DIR/ASSETS-modern.stamp"; then
  echo "ScrapbookUI manifests derived different id-space stamps" >&2
  exit 1
fi

# Let lrpc commit each final package/stamp pair itself; moving the two files
# separately would weaken its failure-atomic pair guarantee. The runtime name
# remains ASSETS.LRP, while platform staging selects the environment package.
"$LRPC" pack "$SCRIPT_DIR/assets/manifest.txt" \
  -o "$SCRIPT_DIR/ASSETS.LRP" \
  --stamp "$SCRIPT_DIR/ASSETS.stamp" \
  --require "$SCRIPT_DIR/assets/scrapbook.pkgreq"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest-modern.txt" \
  -o "$SCRIPT_DIR/assets/ASSETS-modern.LRP" \
  --stamp "$SCRIPT_DIR/assets/ASSETS-modern.stamp" \
  --require "$SCRIPT_DIR/assets/scrapbook.pkgreq"
