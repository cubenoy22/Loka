#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
  echo "Usage: $0 <startup|flip-forward-back> [--update-golden|--ci-structural]" >&2
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  usage
  exit 2
fi

SCENARIO="$1"
case "$SCENARIO" in
  startup|flip-forward-back) ;;
  *) usage; exit 2 ;;
esac

MODE="verify"
if [ $# -eq 2 ]; then
  case "$2" in
    --update-golden) MODE="update" ;;
    --ci-structural) MODE="structural" ;;
    *) usage; exit 2 ;;
  esac
fi

WORK="$PROJECT_DIR/build/macos-scenario/$SCENARIO"
GOLDEN_DIR="$PROJECT_DIR/build/macos-scenario/golden"
GOLDEN="$GOLDEN_DIR/$SCENARIO.png"
GOLDEN_PROFILE="$GOLDEN_DIR/$SCENARIO.profile"
APP="${LOKA_MACOS_SCENARIO_APP:-$PROJECT_DIR/build/macos/Debug/apple/macos/LokaScrapbookScenarioMacOS.app}"
BINARY="$APP/Contents/MacOS/LokaScrapbookScenarioMacOS"
EXPECTED="$PROJECT_DIR/tests/scenarios/expected/scrapbook/$SCENARIO.snap"
SNAP_TOOL="$PROJECT_DIR/tests/scenarios/snaprecord.py"
PNG_TOOL="$PROJECT_DIR/tests/scenarios/pngtool.py"

fail_stage() {
  local stage="$1"
  shift
  echo "$stage stage failed: $*" >&2
  echo "Artifacts: $WORK" >&2
  exit 1
}

if [ ! -x "$BINARY" ]; then
  fail_stage build "missing $BINARY; run: cmake --preset macos-debug && cmake --build --preset macos-scenarios"
fi
if [ ! -f "$EXPECTED" ]; then
  fail_stage record "missing tracked SnapRecord $EXPECTED"
fi

if [ -d "$WORK" ]; then
  rm -rf "$WORK"
fi
mkdir -p "$WORK"
printf 'scenario %s\ncapture_dir %s\n' "$SCENARIO" "$WORK" >"$WORK/LokaTest.cfg"

(
  cd "$WORK"
  "$BINARY" >runner.log 2>&1
) &
APP_PID=$!

cleanup() {
  if kill -0 "$APP_PID" 2>/dev/null; then
    kill "$APP_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

deadline=$((SECONDS + 120))
while kill -0 "$APP_PID" 2>/dev/null; do
  if [ "$SECONDS" -ge "$deadline" ]; then
    fail_stage process "timed out after 120 seconds"
  fi
  sleep 0.1
done
if ! wait "$APP_PID"; then
  fail_stage process "scenario app exited non-zero; see $WORK/runner.log"
fi
trap - EXIT

if [ ! -f "$WORK/complete" ] || [ "$(tr -d '\r\n' <"$WORK/complete")" != "artifacts-ready" ]; then
  fail_stage completion "atomic completion marker is missing or invalid"
fi
for artifact in actual.snap actual.png actual.profile settle-a.png settle-b.png runner.log; do
  if [ ! -f "$WORK/$artifact" ]; then
    fail_stage artifacts "missing $artifact"
  fi
done
if ! python3 "$SNAP_TOOL" compare "$EXPECTED" "$WORK/actual.snap"; then
  fail_stage record "actual.snap differs from $EXPECTED"
fi

if [ "$MODE" = "structural" ]; then
  echo "Scenario structural pass: $SCENARIO"
  echo "Pixel verdict: not evaluated (hosted CI has no persistent rig golden)"
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
if ! python3 "$PNG_TOOL" compare "$WORK/actual.png" "$GOLDEN"; then
  python3 "$PNG_TOOL" diff "$GOLDEN" "$WORK/actual.png" "$WORK/diff.png" || true
  fail_stage golden "actual pixels differ from $GOLDEN"
fi

echo "Scenario passed: $SCENARIO"
