#!/usr/bin/env bash

set -euo pipefail

ACTION="${1:-Verify}"
case "$ACTION" in
  Build|Stage|Verify) ;;
  *)
    echo "Usage: $0 [Build|Stage|Verify]" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$SCRIPT_DIR/presentation-stage.sh"
APP_NAME="LokaScrapbookStandaloneFlowMacOS.app"
BINARY_NAME="LokaScrapbookStandaloneFlowMacOS"
PACKAGED_APP="$SCRIPT_DIR/$APP_NAME"

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

read_audit_line() {
  sed -n "${1}p" "$AUDIT_PATH" | tr -d '\r'
}

assert_success_audit() {
  local line=""
  local line_number=0
  local step=0
  local nonempty_lines=0

  nonempty_lines="$(grep -cve '^[[:space:]]*$' "$AUDIT_PATH" || true)"
  if [[ "$nonempty_lines" -ne 14 ]]; then
    echo "Expected 14 audit lines, found $nonempty_lines." >&2
    return 1
  fi
  if [[ "$(read_audit_line 1)" != "loka_scenario_audit version=1 scenario=standalone-tour" ]]; then
    echo "Unexpected audit header: $(read_audit_line 1)" >&2
    return 1
  fi
  for step in $(seq 1 12); do
    line_number=$((step + 1))
    line="$(read_audit_line "$line_number")"
    if ! printf '%s\n' "$line" | grep -Eq "^step id=${step} .* status=succeeded( |$)"; then
      echo "Unexpected audit step $step: $line" >&2
      return 1
    fi
  done
  if [[ "$(read_audit_line 14)" != "terminal status=succeeded" ]]; then
    echo "Unexpected audit terminal: $(read_audit_line 14)" >&2
    return 1
  fi
}

if [[ -d "$PACKAGED_APP" ]]; then
  IS_PACKAGED_VERIFIER=1
  STAGE_ROOT="$SCRIPT_DIR"
  STAGED_APP="$PACKAGED_APP"
else
  IS_PACKAGED_VERIFIER=0
  PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
  CMAKE_BIN="$(find_cmake || true)"
  if [[ -z "$CMAKE_BIN" ]]; then
    echo "cmake was not found. Install it or add it to PATH." >&2
    exit 1
  fi
  HOST_ARCH="$(uname -m)"
  BUILD_ROOT="$PROJECT_DIR/build/macos/Release"
  BUILT_APP="$BUILD_ROOT/apple/macos/$APP_NAME"
  BUILT_BINARY="$BUILT_APP/Contents/MacOS/$BINARY_NAME"
  BUILT_ASSETS="$BUILT_APP/Contents/Resources/ASSETS.LRP"
  PRESENTATION_ROOT="$PROJECT_DIR/build/presentation"
  STAGE_NAME="macos-${HOST_ARCH}-release"
  STAGE_ROOT="$PRESENTATION_ROOT/$STAGE_NAME"
  STAGED_APP="$STAGE_ROOT/$APP_NAME"

  (
    cd "$PROJECT_DIR"
    "$CMAKE_BIN" --preset macos-release
    "$CMAKE_BIN" --build --preset macos-standalone-flow-release
  )

  if [[ ! -x "$BUILT_BINARY" ]]; then
    echo "Standalone Flow executable not found: $BUILT_BINARY" >&2
    exit 1
  fi
  if [[ ! -f "$BUILT_ASSETS" ]]; then
    echo "Standalone Flow assets not found: $BUILT_ASSETS" >&2
    exit 1
  fi
  if ! /usr/bin/lipo -archs "$BUILT_BINARY" | tr ' ' '\n' | grep -Fxq "$HOST_ARCH"; then
    echo "The standalone executable does not contain the host architecture $HOST_ARCH." >&2
    /usr/bin/lipo -info "$BUILT_BINARY" >&2 || true
    exit 1
  fi

  if [[ "$ACTION" == "Build" ]]; then
    echo "Built $HOST_ARCH Standalone Flow: $BUILT_APP"
    exit 0
  fi

  populate_macos_stage() {
    local destination="$1"
    /usr/bin/ditto "$BUILT_APP" "$destination/$APP_NAME"
    cp "$SCRIPT_DIR/$(basename "${BASH_SOURCE[0]}")" "$destination/Verify-StandaloneFlow.sh"
    cp "$SCRIPT_DIR/presentation-stage.sh" "$destination/presentation-stage.sh"
    chmod +x "$destination/Verify-StandaloneFlow.sh"
    if [[ ! -x "$destination/$APP_NAME/Contents/MacOS/$BINARY_NAME" || \
          ! -f "$destination/$APP_NAME/Contents/Resources/ASSETS.LRP" ]]; then
      echo "The staged application bundle is incomplete." >&2
      return 1
    fi
  }

  loka_replace_stage_directory "$STAGE_ROOT" populate_macos_stage

  if [[ "$ACTION" == "Stage" ]]; then
    echo "Staged $HOST_ARCH presentation: $STAGE_ROOT"
    exit 0
  fi
fi

if [[ "$IS_PACKAGED_VERIFIER" -eq 1 && "$ACTION" != "Verify" ]]; then
  echo "The staged verifier supports only the Verify action." >&2
  exit 1
fi

STAGED_BINARY="$STAGED_APP/Contents/MacOS/$BINARY_NAME"
STAGED_ASSETS="$STAGED_APP/Contents/Resources/ASSETS.LRP"
AUDIT_PATH="$STAGE_ROOT/LOG.TXT"
RUNNER_LOG="$STAGE_ROOT/runner.log"
if [[ ! -x "$STAGED_BINARY" ]]; then
  echo "Staged executable not found: $STAGED_BINARY" >&2
  exit 1
fi
if [[ ! -f "$STAGED_ASSETS" ]]; then
  echo "Staged assets not found: $STAGED_ASSETS" >&2
  exit 1
fi

rm -f "$AUDIT_PATH" "$RUNNER_LOG"
(
  cd "$STAGE_ROOT"
  exec "$STAGED_BINARY" >"$RUNNER_LOG" 2>&1
) &
APP_PID=$!

cleanup() {
  if kill -0 "$APP_PID" 2>/dev/null; then
    kill "$APP_PID" 2>/dev/null || true
  fi
  wait "$APP_PID" 2>/dev/null || true
}
trap cleanup EXIT

deadline=$((SECONDS + 30))
while [[ "$SECONDS" -lt "$deadline" ]]; do
  if [[ -f "$AUDIT_PATH" ]]; then
    if grep -Eq '^terminal status=(failed|canceled)\r?$' "$AUDIT_PATH"; then
      echo "Standalone Flow reported a failed or canceled terminal status." >&2
      exit 1
    fi
    if grep -Eq '^terminal status=succeeded\r?$' "$AUDIT_PATH"; then
      assert_success_audit
      echo "Runtime-verified macOS Standalone Flow: $AUDIT_PATH"
      exit 0
    fi
  fi
  if ! kill -0 "$APP_PID" 2>/dev/null; then
    if wait "$APP_PID"; then
      exit_code=0
    else
      exit_code=$?
    fi
    echo "Standalone Flow exited before publishing a success audit (exit code $exit_code)." >&2
    exit 1
  fi
  sleep 0.2
done

echo "Timed out after 30 seconds waiting for Standalone Flow success." >&2
exit 1
