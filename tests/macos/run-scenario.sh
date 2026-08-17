#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
  echo "Usage: $0 <example> <scenario from scenarios.txt> [--update-golden|--ci-structural|--inspect]" >&2
}

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  usage
  exit 2
fi

EXAMPLE="$1"
SCENARIO="$2"
SCENARIO_REGISTRY="$PROJECT_DIR/tests/scenarios/scenarios.txt"
if [[ ! "$EXAMPLE" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || [[ ! "$SCENARIO" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || ! grep -Fxq -- "$EXAMPLE $SCENARIO" "$SCENARIO_REGISTRY"; then
  usage
  exit 2
fi

MODE="verify"
RUN_MODE="flow"
if [ $# -eq 3 ]; then
  case "$3" in
    --update-golden) MODE="update" ;;
    --ci-structural) MODE="structural" ;;
    --inspect) MODE="inspect"; RUN_MODE="inspect" ;;
    *) usage; exit 2 ;;
  esac
fi

WORK="${LOKA_MACOS_SCENARIO_WORK:-$PROJECT_DIR/build/macos-scenario/$EXAMPLE/$SCENARIO}"
GOLDEN_DIR="$PROJECT_DIR/build/macos-scenario/golden/$EXAMPLE"
GOLDEN="$GOLDEN_DIR/$SCENARIO.png"
GOLDEN_PROFILE="$GOLDEN_DIR/$SCENARIO.profile"
case "$EXAMPLE" in
  scrapbook) TARGET="LokaScrapbookScenarioMacOS" ;;
  helloworld) TARGET="LokaHelloWorldScenarioMacOS" ;;
  tutorial) TARGET="LokaTutorialScenarioMacOS" ;;
  minesweeper) TARGET="LokaMineSweeperScenarioMacOS" ;;
  floppybird) TARGET="LokaFloppyBirdScenarioMacOS" ;;
  *) usage; exit 2 ;;
esac
APP="${LOKA_MACOS_SCENARIO_APP:-$PROJECT_DIR/build/macos/Debug/apple/macos/$TARGET.app}"
BINARY="$APP/Contents/MacOS/$TARGET"
EXPECTED="$PROJECT_DIR/tests/scenarios/expected/$EXAMPLE/$SCENARIO.audit"
PNG_TOOL="$PROJECT_DIR/tests/scenarios/pngtool.py"
WORK_DIR_TOOL="$PROJECT_DIR/tests/macos/validate-work-dir.py"
PYTHON3="${PYTHON3:-python3}"
SCREENCAPTURE="${SCREENCAPTURE:-/usr/sbin/screencapture}"

fail_stage() {
  local stage="$1"
  shift
  echo "$stage stage failed: $*" >&2
  echo "Artifacts: $WORK" >&2
  exit 1
}

publish_verified() {
  printf 'runner-verified\n' >"$WORK/verified.tmp"
  mv -f "$WORK/verified.tmp" "$WORK/verified"
}

if [ ! -x "$BINARY" ]; then
  fail_stage build "missing $BINARY; run: cmake --preset macos-debug && cmake --build --preset macos-scenarios"
fi
if [ ! -f "$EXPECTED" ]; then
  fail_stage verdict "missing tracked audit $EXPECTED"
fi
if ! WORK="$("$PYTHON3" "$WORK_DIR_TOOL" "$PROJECT_DIR/build" "$WORK")"; then
  fail_stage setup "work directory must resolve strictly below $PROJECT_DIR/build"
fi

if [ -d "$WORK" ]; then
  rm -rf "$WORK"
fi
mkdir -p "$WORK"
printf 'scenario %s\ncapture_dir %s\n' "$SCENARIO" "$WORK" >"$WORK/LokaTest.cfg"

if [ "${LOKA_MACOS_SCENARIO_CAPTURE_DESKTOP:-0}" = "1" ]; then
  if ! "$SCREENCAPTURE" -x "$WORK/desktop-before.png"; then
    fail_stage capture "could not capture desktop-before.png"
  fi
fi

(
  cd "$WORK"
  LOKA_MACOS_SCENARIO_MODE="$RUN_MODE" exec "$BINARY" >runner.log 2>&1
) &
APP_PID=$!
printf '%s\n' "$APP_PID" >"$WORK/app.pid"

cleanup() {
  if [ "${LOKA_MACOS_SCENARIO_RETAIN_ON_FAILURE:-0}" = "1" ]; then
    echo "Retaining scenario app pid $APP_PID after failure" >&2
    return
  fi
  if kill -0 "$APP_PID" 2>/dev/null; then
    kill "$APP_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

deadline=$((SECONDS + 120))
HELD_CAPTURED=0
while kill -0 "$APP_PID" 2>/dev/null; do
  if [ "$SECONDS" -ge "$deadline" ]; then
    fail_stage process "timed out after 120 seconds"
  fi
  if [ "$MODE" = "inspect" ] && [ "$HELD_CAPTURED" -eq 0 ] && [ -f "$WORK/ready" ]; then
    if [ "$(tr -d '\r\n' <"$WORK/ready")" != "inspection-ready" ]; then
      fail_stage ready "atomic inspect marker is invalid"
    fi
    if [ "${LOKA_MACOS_SCENARIO_CAPTURE_DESKTOP:-0}" = "1" ]; then
      if ! "$SCREENCAPTURE" -x "$WORK/desktop-after.png"; then
        fail_stage capture "could not capture held desktop-after.png"
      fi
    fi
    HELD_CAPTURED=1
  fi
  sleep 0.1
done
if ! wait "$APP_PID"; then
  fail_stage process "scenario app exited non-zero; see $WORK/runner.log"
fi
trap - EXIT

if [ "${LOKA_MACOS_SCENARIO_CAPTURE_DESKTOP:-0}" = "1" ] && [ "$MODE" != "inspect" ]; then
  if ! "$SCREENCAPTURE" -x "$WORK/desktop-after.png"; then
    fail_stage capture "could not capture desktop-after.png"
  fi
fi

if [ ! -f "$WORK/complete" ] || [ "$(tr -d '\r\n' <"$WORK/complete")" != "artifacts-ready" ]; then
  fail_stage completion "atomic completion marker is missing or invalid"
fi
for artifact in actual.audit actual.png actual.profile settle-a.png settle-b.png runner.log; do
  if [ ! -f "$WORK/$artifact" ]; then
    fail_stage artifacts "missing $artifact"
  fi
done
if ! cmp "$EXPECTED" "$WORK/actual.audit"; then
  fail_stage verdict "actual.audit differs from $EXPECTED"
fi

if [ "$MODE" = "inspect" ]; then
  if [ "$HELD_CAPTURED" -eq 0 ]; then
    fail_stage ready "inspect run exited without a ready marker"
  fi
  echo "Scenario inspect pass: $EXAMPLE $SCENARIO"
  echo "Pixel verdict: not evaluated (inspect uses human presentation evidence)"
  publish_verified
  exit 0
fi

if [ "$MODE" = "structural" ]; then
  echo "Scenario structural pass: $EXAMPLE $SCENARIO"
  echo "Pixel verdict: not evaluated (hosted CI has no persistent rig golden)"
  publish_verified
  exit 0
fi

if [ "$MODE" = "update" ]; then
  mkdir -p "$GOLDEN_DIR"
  cp -f "$WORK/actual.png" "$GOLDEN"
  cp -f "$WORK/actual.profile" "$GOLDEN_PROFILE"
  echo "Updated macOS rig golden: $GOLDEN"
  exit 0
fi

if [ ! -f "$GOLDEN" ] || [ ! -f "$GOLDEN_PROFILE" ]; then
  fail_stage golden "missing rig-local golden/profile; rerun with --update-golden"
fi
if ! cmp -s "$WORK/actual.profile" "$GOLDEN_PROFILE"; then
  fail_stage profile "rig profile differs from $GOLDEN_PROFILE"
fi
if ! "$PYTHON3" "$PNG_TOOL" compare "$WORK/actual.png" "$GOLDEN"; then
  "$PYTHON3" "$PNG_TOOL" diff "$GOLDEN" "$WORK/actual.png" "$WORK/diff.png" || true
  fail_stage golden "actual pixels differ from $GOLDEN"
fi

echo "Scenario passed: $EXAMPLE $SCENARIO"
publish_verified
