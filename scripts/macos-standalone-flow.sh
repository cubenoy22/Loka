#!/usr/bin/env bash

set -euo pipefail

ACTION="${1:-Verify}"
case "$ACTION" in
  Build|Stage|Verify|Release) ;;
  *)
    echo "Usage: $0 [Build|Stage|Verify|Release]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Keep AppKit timers advancing when a legacy VM has no foreground input. The
# guard makes caffeinate own exactly one verifier process lifetime.
if [[ "$ACTION" == "Verify" && "${LOKA_STANDALONE_CAFFEINATED:-0}" != "1" && -x /usr/bin/caffeinate ]]; then
  export LOKA_STANDALONE_CAFFEINATED=1
  exec /usr/bin/caffeinate -disu /bin/bash "$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")" "$ACTION"
fi
. "$SCRIPT_DIR/presentation-stage.sh"
CATALOG_NAME="standalone-flow-catalog.tsv"
VERIFY_TIMEOUT_SECONDS=120

find_cmake() {
  local candidate=""
  if command -v cmake >/dev/null 2>&1; then
    command -v cmake
    return 0
  fi
  for candidate in /opt/homebrew/bin/cmake /usr/local/bin/cmake /opt/local/bin/cmake; do
    if [[ -x "$candidate" ]]; then
      echo "$candidate"
      return 0
    fi
  done
  return 1
}

validate_catalog() {
  local catalog="$1"
  local key=""
  local target=""
  local expected=""
  local count=0

  if [[ ! -f "$catalog" ]]; then
    echo "Standalone Flow catalog not found: $catalog" >&2
    return 1
  fi
  while IFS=$'\t' read -r key target expected; do
    case "$key" in
      ""|\#*) continue ;;
    esac
    if [[ -z "$target" || -z "$expected" ]]; then
      echo "Invalid Standalone Flow catalog entry for '$key'." >&2
      return 1
    fi
    case "$key" in
      *[!a-z0-9_-]*)
        echo "Invalid Standalone Flow catalog key: $key" >&2
        return 1
        ;;
    esac
    case "$target" in
      *[!A-Za-z0-9_]*)
        echo "Invalid Standalone Flow catalog target for '$key': $target" >&2
        return 1
        ;;
    esac
    case "$expected" in
      tests/scenarios/expected/*.audit) ;;
      *)
        echo "Invalid Standalone Flow expected audit for '$key': $expected" >&2
        return 1
        ;;
    esac
    if [[ "$expected" == /* || "$expected" == *..* ]]; then
      echo "Invalid Standalone Flow expected audit for '$key': $expected" >&2
      return 1
    fi
    count=$((count + 1))
  done <"$catalog"
  if [[ "$count" -ne 5 ]]; then
    echo "Standalone Flow catalog must contain five runnable applications; found $count." >&2
    return 1
  fi
}

visit_catalog() {
  local catalog="$1"
  local callback="$2"
  local key=""
  local target=""
  local expected=""

  validate_catalog "$catalog"
  while IFS=$'\t' read -r key target expected; do
    case "$key" in
      ""|\#*) continue ;;
    esac
    "$callback" "$key" "$target" "$expected"
  done <"$catalog"
}

assert_success_audit() {
  local expected="$1"
  local actual="$2"
  if ! cmp -s "$expected" "$actual"; then
    echo "Standalone audit does not match the tracked expected audit: $expected" >&2
    cmp "$expected" "$actual" >&2 || true
    return 1
  fi
}

if [[ -f "$SCRIPT_DIR/$CATALOG_NAME" ]]; then
  IS_PACKAGED_VERIFIER=1
  STAGE_ROOT="$SCRIPT_DIR"
  CATALOG="$STAGE_ROOT/$CATALOG_NAME"
else
  IS_PACKAGED_VERIFIER=0
  PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
  CMAKE_BIN="$(find_cmake || true)"
  if [[ -z "$CMAKE_BIN" ]]; then
    echo "cmake was not found. Install it or add it to PATH." >&2
    exit 1
  fi
  PATH="$(dirname "$CMAKE_BIN"):$PATH"
  export PATH
  if [[ -z "${LOKA_LIPO_BIN:-}" ]]; then
    for lipo_candidate in \
      /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/lipo \
      /usr/bin/lipo; do
      if [[ -x "$lipo_candidate" ]]; then
        LOKA_LIPO_BIN="$lipo_candidate"
        export LOKA_LIPO_BIN
        break
      fi
    done
  fi
  HOST_ARCH="$(uname -m)"
  TARGET_ARCH="${LOKA_STANDALONE_MACOS_ARCH:-$HOST_ARCH}"
  case "$TARGET_ARCH" in
    arm64|x86_64|i386) ;;
    *)
      echo "Unsupported macOS Standalone architecture: $TARGET_ARCH" >&2
      exit 1
      ;;
  esac
  if [[ "$TARGET_ARCH" == "$HOST_ARCH" ]]; then
    BUILD_ROOT="$PROJECT_DIR/build/macos/Release"
  else
    BUILD_ROOT="$PROJECT_DIR/build/macos/$TARGET_ARCH/Release"
  fi
  BUILD_APP_ROOT="$BUILD_ROOT/apple/macos"
  BUILD_CATALOG="$BUILD_APP_ROOT/$CATALOG_NAME"
  if [[ "$ACTION" == "Release" ]]; then
    STAGE_ROOT="$PROJECT_DIR/build/release/macos-${TARGET_ARCH}"
    BUILD_TARGET="LokaStandaloneLoopMacOSAll"
  else
    STAGE_ROOT="$PROJECT_DIR/build/presentation/macos-${TARGET_ARCH}-release"
    BUILD_TARGET="LokaStandaloneFlowMacOSAll"
  fi

  (
    cd "$PROJECT_DIR"
    if [[ "$TARGET_ARCH" == "$HOST_ARCH" ]]; then
      "$CMAKE_BIN" --preset macos-release
    else
      "$CMAKE_BIN" --preset macos-release \
        -B "$BUILD_ROOT" \
        -DCMAKE_OSX_ARCHITECTURES="$TARGET_ARCH"
    fi
    "$CMAKE_BIN" --build "$BUILD_ROOT" --target "$BUILD_TARGET"
  )

  validate_built_app() {
    local key="$1"
    local target="$2"
    local expected="$3"
    local app="$BUILD_APP_ROOT/$target.app"
    local binary="$app/Contents/MacOS/$target"
    : "$expected"
    if [[ ! -x "$binary" ]]; then
      echo "Standalone Flow executable not found for $key: $binary" >&2
      return 1
    fi
    if ! loka_binary_contains_arch "$binary" "$TARGET_ARCH"; then
      echo "The $key standalone executable does not contain the target architecture $TARGET_ARCH." >&2
      "${LOKA_LIPO_BIN:-/usr/bin/lipo}" -info "$binary" >&2 || true
      return 1
    fi
  }
  if [[ "$ACTION" == "Release" ]]; then
    validate_loop_app() {
      local key="$1"
      local target="$2"
      local expected="$3"
      local loop_target="${target/StandaloneFlow/StandaloneLoop}"
      local binary="$BUILD_APP_ROOT/$loop_target.app/Contents/MacOS/$loop_target"
      : "$expected"
      if [[ ! -x "$binary" ]]; then
        echo "Autonomous loop executable not found for $key: $binary" >&2
        return 1
      fi
      if ! loka_binary_contains_arch "$binary" "$TARGET_ARCH"; then
        echo "The $key loop executable does not contain $TARGET_ARCH." >&2
        return 1
      fi
    }
    visit_catalog "$BUILD_CATALOG" validate_loop_app

    SIMPLE_VIEWER="$BUILD_ROOT/example/SimpleViewer/LokaSimpleViewerMacOS"
    if [[ ! -x "$SIMPLE_VIEWER" ]]; then
      echo "SimpleViewer Release executable not found: $SIMPLE_VIEWER" >&2
      exit 1
    fi
    if ! loka_binary_contains_arch "$SIMPLE_VIEWER" "$TARGET_ARCH"; then
      echo "SimpleViewer does not contain the target architecture $TARGET_ARCH." >&2
      exit 1
    fi

    populate_macos_release() {
      local destination="$1"
      copy_loop_entry() {
        local key="$1"
        local target="$2"
        local expected="$3"
        local loop_target="${target/StandaloneFlow/StandaloneLoop}"
        : "$key" "$expected"
        /usr/bin/ditto "$BUILD_APP_ROOT/$loop_target.app" "$destination/$loop_target.app"
      }
      visit_catalog "$BUILD_CATALOG" copy_loop_entry
      cp "$SIMPLE_VIEWER" "$destination/LokaSimpleViewerMacOS"
      chmod +x "$destination/LokaSimpleViewerMacOS"
      printf '%s\n' \
        'Loka 0.0.4 Release applications' \
        '' \
        'The five StandaloneLoop applications run their UI tour repeatedly.' \
        'Quit a loop application to stop it. LokaSimpleViewerMacOS remains interactive.' \
        >"$destination/README.txt"
    }
    loka_replace_stage_directory "$STAGE_ROOT" populate_macos_release
    echo "Staged five autonomous loops plus SimpleViewer ($TARGET_ARCH): $STAGE_ROOT"
    exit 0
  fi

  visit_catalog "$BUILD_CATALOG" validate_built_app

  if [[ "$ACTION" == "Build" ]]; then
    echo "Built five $TARGET_ARCH Standalone Flow applications under $BUILD_APP_ROOT"
    exit 0
  fi

  populate_macos_stage() {
    local destination="$1"
    mkdir -p "$destination/expected"
    cp "$BUILD_CATALOG" "$destination/$CATALOG_NAME"
    cp "$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")" "$destination/Verify-StandaloneFlow.sh"
    cp "$SCRIPT_DIR/presentation-stage.sh" "$destination/presentation-stage.sh"
    chmod +x "$destination/Verify-StandaloneFlow.sh"

    copy_stage_entry() {
      local key="$1"
      local target="$2"
      local expected="$3"
      /usr/bin/ditto "$BUILD_APP_ROOT/$target.app" "$destination/$target.app"
      cp "$PROJECT_DIR/$expected" "$destination/expected/$key.audit"
    }
    visit_catalog "$BUILD_CATALOG" copy_stage_entry
  }

  loka_replace_stage_directory "$STAGE_ROOT" populate_macos_stage
  CATALOG="$STAGE_ROOT/$CATALOG_NAME"

  if [[ "$ACTION" == "Stage" ]]; then
    echo "Staged five $TARGET_ARCH Standalone Flow applications: $STAGE_ROOT"
    exit 0
  fi
fi

if [[ "$IS_PACKAGED_VERIFIER" -eq 1 && "$ACTION" != "Verify" ]]; then
  echo "The staged verifier supports only the Verify action." >&2
  exit 1
fi

validate_catalog "$CATALOG"
ACTUAL_ROOT="$STAGE_ROOT/actual"
mkdir -p "$ACTUAL_ROOT"
ACTIVE_APP_PID=""

reset_actual_entry() {
  local key="$1"
  local target="$2"
  local expected="$3"
  : "$target" "$expected"
  rm -f "$ACTUAL_ROOT/$key.audit" "$ACTUAL_ROOT/$key.runner.log"
}
visit_catalog "$CATALOG" reset_actual_entry

stop_active_app() {
  if [[ -n "$ACTIVE_APP_PID" ]] && kill -0 "$ACTIVE_APP_PID" 2>/dev/null; then
    kill "$ACTIVE_APP_PID" 2>/dev/null || true
  fi
  if [[ -n "$ACTIVE_APP_PID" ]]; then
    wait "$ACTIVE_APP_PID" 2>/dev/null || true
  fi
  ACTIVE_APP_PID=""
}
trap stop_active_app EXIT

verify_stage_entry() {
  local key="$1"
  local target="$2"
  local expected_source="$3"
  local app="$STAGE_ROOT/$target.app"
  local binary="$app/Contents/MacOS/$target"
  local expected="$STAGE_ROOT/expected/$key.audit"
  local audit="$STAGE_ROOT/LOG.TXT"
  local actual="$ACTUAL_ROOT/$key.audit"
  local runner_log="$ACTUAL_ROOT/$key.runner.log"
  local deadline=0
  local exit_code=0
  : "$expected_source"

  if [[ ! -x "$binary" ]]; then
    echo "Staged executable not found for $key: $binary" >&2
    return 1
  fi
  if [[ ! -f "$expected" ]]; then
    echo "Staged expected audit not found for $key: $expected" >&2
    return 1
  fi

  rm -f "$audit" "$actual" "$runner_log"
  (
    cd "$STAGE_ROOT"
    exec "$binary" >"$runner_log" 2>&1
  ) &
  ACTIVE_APP_PID=$!

  deadline=$((SECONDS + VERIFY_TIMEOUT_SECONDS))
  while [[ "$SECONDS" -lt "$deadline" ]]; do
    if [[ -f "$audit" ]]; then
      if grep -Eq '^terminal status=(failed|canceled)\r?$' "$audit"; then
        cp "$audit" "$actual"
        echo "$key Standalone Flow reported a failed or canceled terminal status." >&2
        stop_active_app
        return 1
      fi
      if grep -Eq '^terminal status=succeeded\r?$' "$audit"; then
        cp "$audit" "$actual"
        if ! assert_success_audit "$expected" "$actual"; then
          stop_active_app
          return 1
        fi
        stop_active_app
        echo "Runtime-verified macOS Standalone Flow: $key"
        return 0
      fi
    fi
    if ! kill -0 "$ACTIVE_APP_PID" 2>/dev/null; then
      if wait "$ACTIVE_APP_PID"; then
        exit_code=0
      else
        exit_code=$?
      fi
      echo "$key Standalone Flow exited before publishing a success audit (exit code $exit_code)." >&2
      return 1
    fi
    sleep 0.2
  done

  echo "Timed out after $VERIFY_TIMEOUT_SECONDS seconds waiting for $key Standalone Flow success." >&2
  stop_active_app
  return 1
}

visit_catalog "$CATALOG" verify_stage_entry
rm -f "$STAGE_ROOT/LOG.TXT"
echo "Runtime-verified all five macOS Standalone Flow applications: $ACTUAL_ROOT"
