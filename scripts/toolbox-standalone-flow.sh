#!/usr/bin/env bash

set -euo pipefail

ACTION="${1:-Stage}"
case "$ACTION" in
  Build|Stage) ;;
  *)
    echo "Usage: $0 [Build|Stage]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/presentation-stage.sh"

find_cmake() {
  local candidate=""
  if command -v cmake >/dev/null 2>&1; then
    command -v cmake
    return 0
  fi
  for candidate in /opt/homebrew/bin/cmake /usr/local/bin/cmake; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

CMAKE_BIN="$(find_cmake || true)"
if [[ -z "$CMAKE_BIN" ]]; then
  echo "cmake was not found. Install it or add it to PATH." >&2
  exit 1
fi

BUILD_ROOT="$PROJECT_DIR/build/retro68/68k/Release/tests/toolbox"
BUILT_APPLICATION="$BUILD_ROOT/LokaScrapbookStandaloneFlow68K.bin"
BUILT_ASSETS="$BUILD_ROOT/ASSETS.LRP"
STAGE_ROOT="$PROJECT_DIR/build/presentation/toolbox-68k-release"

(
  cd "$PROJECT_DIR"
  "$CMAKE_BIN" --preset retro68-68k-release
  "$CMAKE_BIN" --build --preset retro68-68k-release \
    --target LokaScrapbookStandaloneFlow68K_APPL
)

if [[ ! -s "$BUILT_APPLICATION" ]]; then
  echo "Standalone Flow MacBinary not found: $BUILT_APPLICATION" >&2
  exit 1
fi
if [[ ! -s "$BUILT_ASSETS" ]]; then
  echo "Standalone Flow assets not found: $BUILT_ASSETS" >&2
  exit 1
fi
if ! cmp -s "$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP" "$BUILT_ASSETS"; then
  echo "Built ASSETS.LRP does not match the application package." >&2
  exit 1
fi

if [[ "$ACTION" == "Build" ]]; then
  echo "Built Toolbox 68K Standalone Flow: $BUILT_APPLICATION"
  exit 0
fi

populate_toolbox_stage() {
  local destination="$1"
  cp "$BUILT_APPLICATION" "$destination/LokaScrapbookStandaloneFlow68K.bin"
  cp "$BUILT_ASSETS" "$destination/ASSETS.LRP"
  if ! cmp -s "$BUILT_APPLICATION" "$destination/LokaScrapbookStandaloneFlow68K.bin" \
    || ! cmp -s "$BUILT_ASSETS" "$destination/ASSETS.LRP"; then
    echo "The staged Toolbox presentation is incomplete." >&2
    return 1
  fi
}

loka_replace_stage_directory "$STAGE_ROOT" populate_toolbox_stage
echo "Staged Toolbox 68K presentation: $STAGE_ROOT"
