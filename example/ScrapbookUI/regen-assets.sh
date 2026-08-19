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
  --header "$BAKE_DIR/R-classic.hpp"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest-modern.txt" \
  -o "$BAKE_DIR/ASSETS-modern.LRP" \
  --header "$BAKE_DIR/R-modern.hpp"

if ! cmp -s "$BAKE_DIR/R-classic.hpp" "$BAKE_DIR/R-modern.hpp"; then
  echo "ScrapbookUI manifests generated different resource headers" >&2
  exit 1
fi

# Let lrpc commit each final package/header pair itself; moving the two files
# separately would weaken its failure-atomic pair guarantee. The runtime name
# remains ASSETS.LRP, while platform staging selects the environment package.
"$LRPC" pack "$SCRIPT_DIR/assets/manifest.txt" \
  -o "$SCRIPT_DIR/ASSETS.LRP" \
  --header "$SCRIPT_DIR/src/R.hpp"
"$LRPC" pack "$SCRIPT_DIR/assets/manifest-modern.txt" \
  -o "$SCRIPT_DIR/assets/ASSETS-modern.LRP" \
  --header "$SCRIPT_DIR/src/R.hpp"
