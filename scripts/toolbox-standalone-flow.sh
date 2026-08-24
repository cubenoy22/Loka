#!/usr/bin/env bash

set -euo pipefail

ACTION="${1:-Stage}"
case "$ACTION" in
  Build|Stage|Release) ;;
  *)
    echo "Usage: $0 [Build|Stage|Release]" >&2
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

find_retro68_tool() {
  local name="$1"
  local candidate=""

  # Keep this lookup order aligned with mame-dev-disk.sh. The standalone
  # stage must also work before a MAME environment has been configured.
  if [[ -n "${RETRO68_TOOLCHAIN_BIN:-}" \
    && -x "$RETRO68_TOOLCHAIN_BIN/$name" ]]; then
    echo "$RETRO68_TOOLCHAIN_BIN/$name"
    return 0
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return 0
  fi
  if [[ -n "${RETRO68_BUILD_DIR:-}" \
    && -x "$RETRO68_BUILD_DIR/toolchain/bin/$name" ]]; then
    echo "$RETRO68_BUILD_DIR/toolchain/bin/$name"
    return 0
  fi
  for candidate in \
    "$HOME/Retro68-build/toolchain/bin/$name" \
    "$HOME/Documents/Projects/Retro68-build/toolchain/bin/$name"; do
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
BUILT_DISK="$BUILD_ROOT/LokaScrapbookStandaloneFlow68K.dsk"
BUILT_ASSETS="$BUILD_ROOT/ASSETS.LRP"
STAGE_README="$PROJECT_DIR/docs/TOOLBOX_STANDALONE_FLOW.md"
if [[ "$ACTION" == "Release" ]]; then
  BUILD_TARGET="LokaStandaloneLoop68KAll"
  STAGE_ROOT="$PROJECT_DIR/build/release/toolbox-68k"
else
  BUILD_TARGET="LokaScrapbookStandaloneFlow68K_APPL"
  STAGE_ROOT="$PROJECT_DIR/build/presentation/toolbox-68k-release"
fi

(
  cd "$PROJECT_DIR"
  "$CMAKE_BIN" --preset retro68-68k-release
  "$CMAKE_BIN" --build --preset retro68-68k-release \
    --target "$BUILD_TARGET"
)

if [[ ! -s "$BUILT_ASSETS" ]]; then
  echo "Standalone Flow assets not found: $BUILT_ASSETS" >&2
  exit 1
fi
if [[ ! -s "$STAGE_README" ]]; then
  echo "Standalone Flow stage README not found: $STAGE_README" >&2
  exit 1
fi
if ! cmp -s "$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP" "$BUILT_ASSETS"; then
  echo "Built ASSETS.LRP does not match the application package." >&2
  exit 1
fi

if [[ "$ACTION" != "Release" ]]; then
  if [[ ! -s "$BUILT_APPLICATION" ]]; then
    echo "Standalone Flow MacBinary not found: $BUILT_APPLICATION" >&2
    exit 1
  fi
  if [[ ! -s "$BUILT_DISK" ]]; then
    echo "Standalone Flow HFS disk not found: $BUILT_DISK" >&2
    exit 1
  fi
fi

if [[ "$ACTION" == "Build" ]]; then
  echo "Built Toolbox 68K Standalone Flow: $BUILT_APPLICATION"
  exit 0
fi

HMOUNT="$(find_retro68_tool hmount || true)"
HCOPY="$(find_retro68_tool hcopy || true)"
HLS="$(find_retro68_tool hls || true)"
HUMOUNT="$(find_retro68_tool humount || true)"
if [[ -z "$HMOUNT" || -z "$HCOPY" || -z "$HLS" || -z "$HUMOUNT" ]]; then
  echo "Retro68 hmount, hcopy, hls, and humount are required to stage the HFS disk." >&2
  exit 1
fi

populate_scrapbook_disk() (
  set -euo pipefail

  local disk="$1"
  local application_name="$2"
  local work_root="$3"
  local hfs_home=""
  local extracted_assets=""
  local listing=""

  hfs_home="$(mktemp -d "$work_root/.hfsutils.XXXXXX")"
  extracted_assets="$hfs_home/ASSETS.LRP"

  unmount_and_clean() {
    local exit_code=$?
    HOME="$hfs_home" "$HUMOUNT" >/dev/null 2>&1 || true
    rm -rf "$hfs_home"
    return "$exit_code"
  }
  trap unmount_and_clean EXIT

  HOME="$hfs_home" "$HMOUNT" "$disk" >/dev/null
  HOME="$hfs_home" "$HCOPY" -r "$BUILT_ASSETS" :
  listing="$(HOME="$hfs_home" "$HLS" -1 :)"
  if ! grep -Fxq "$application_name" <<<"$listing" \
    || ! grep -Fxq 'ASSETS.LRP' <<<"$listing"; then
    echo "The staged HFS disk does not contain the application and ASSETS.LRP." >&2
    return 1
  fi
  HOME="$hfs_home" "$HCOPY" -r ':ASSETS.LRP' "$extracted_assets"
  if ! cmp -s "$BUILT_ASSETS" "$extracted_assets"; then
    echo "ASSETS.LRP changed while it was copied into the staged HFS disk." >&2
    return 1
  fi
  HOME="$hfs_home" "$HUMOUNT"
  rm -rf "$hfs_home"
  trap - EXIT
)

populate_toolbox_stage() {
  local destination="$1"
  cp "$BUILT_APPLICATION" "$destination/LokaScrapbookStandaloneFlow68K.bin"
  cp "$BUILT_DISK" "$destination/LokaScrapbookStandaloneFlow68K.dsk"
  cp "$BUILT_ASSETS" "$destination/ASSETS.LRP"
  cp "$STAGE_README" "$destination/README.md"
  populate_scrapbook_disk \
    "$destination/LokaScrapbookStandaloneFlow68K.dsk" \
    LokaScrapbookStandaloneFlow68K \
    "$destination"
  if ! cmp -s "$BUILT_APPLICATION" "$destination/LokaScrapbookStandaloneFlow68K.bin" \
    || ! cmp -s "$BUILT_ASSETS" "$destination/ASSETS.LRP" \
    || ! cmp -s "$STAGE_README" "$destination/README.md"; then
    echo "The staged Toolbox presentation is incomplete." >&2
    return 1
  fi
}

populate_toolbox_release() {
  local destination="$1"
  local artifact=""
  local built_root=""
  local built_name=""
  local release_names=(
    LokaScrapbookStandaloneLoop68K
    LokaHelloStandaloneLoop68K
    LokaTutorialStandaloneLoop68K
    LokaMineStandaloneLoop68K
    LokaFloppyStandaloneLoop68K
  )

  for built_name in "${release_names[@]}"; do
    built_root="$BUILD_ROOT/$built_name"
    for artifact in bin dsk; do
      if [[ ! -s "$built_root.$artifact" ]]; then
        echo "Toolbox Release artifact not found: $built_root.$artifact" >&2
        return 1
      fi
      cp "$built_root.$artifact" "$destination/$built_name.$artifact"
    done
  done

  built_root="$PROJECT_DIR/build/retro68/68k/Release/example/SimpleViewer/LokaSimpleViewer68K"
  for artifact in bin dsk; do
    if [[ ! -s "$built_root.$artifact" ]]; then
      echo "SimpleViewer Release artifact not found: $built_root.$artifact" >&2
      return 1
    fi
    cp "$built_root.$artifact" "$destination/LokaSimpleViewer68K.$artifact"
  done

  cp "$BUILT_ASSETS" "$destination/ASSETS.LRP"
  cp "$STAGE_README" "$destination/README.md"
  populate_scrapbook_disk \
    "$destination/LokaScrapbookStandaloneLoop68K.dsk" \
    LokaScrapbookStandaloneLoop68K \
    "$destination"
}

if [[ "$ACTION" == "Release" ]]; then
  loka_replace_stage_directory "$STAGE_ROOT" populate_toolbox_release
  echo "Staged five autonomous Toolbox loops plus SimpleViewer: $STAGE_ROOT"
  exit 0
fi

loka_replace_stage_directory "$STAGE_ROOT" populate_toolbox_stage
echo "Staged Toolbox 68K presentation: $STAGE_ROOT"
