#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUNNER="$REPO_DIR/tests/toolbox/run-scenario.sh"
LAUNCHER="$REPO_DIR/tests/toolbox/mame-launch.lua"
SANDBOX="$(mktemp -d)"
export SANDBOX
trap 'rm -rf "$SANDBOX"' EXIT

fail() {
  echo "ToolboxScenarioRunnerTest failed: $*" >&2
  exit 1
}

mkdir -p \
  "$SANDBOX/repo/tests/toolbox" \
  "$SANDBOX/repo/tests/scenarios/expected/scrapbook" \
  "$SANDBOX/repo/tests/scenarios/expected/helloworld" \
  "$SANDBOX/repo/tests/scenarios/expected/tutorial" \
  "$SANDBOX/repo/tests/scenarios/expected/minesweeper" \
  "$SANDBOX/repo/tests/scenarios/expected/floppybird" \
  "$SANDBOX/repo/scripts" \
  "$SANDBOX/repo/example/ScrapbookUI" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox" \
  "$SANDBOX/retro-tools"
cp "$RUNNER" "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
cp "$LAUNCHER" "$SANDBOX/repo/tests/toolbox/mame-launch.lua"
cp "$REPO_DIR/tests/scenarios/pngtool.py" "$SANDBOX/repo/tests/scenarios/pngtool.py"
cp "$REPO_DIR/tests/scenarios/scrapbook-package-fixtures.txt" \
  "$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt"
cp "$REPO_DIR/tests/scenarios/expected/scrapbook/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/scrapbook/startup.audit"
cp "$REPO_DIR/tests/scenarios/expected/helloworld/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/helloworld/startup.audit"
cp "$REPO_DIR/tests/scenarios/expected/tutorial/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/tutorial/startup.audit"
cp "$REPO_DIR/tests/scenarios/expected/minesweeper/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/minesweeper/startup.audit"
cp "$REPO_DIR/tests/scenarios/expected/floppybird/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/floppybird/startup.audit"
cp "$REPO_DIR/tests/scenarios/expected/tutorial/increment-summary-toggle.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/tutorial/increment-summary-toggle.audit"
cp "$REPO_DIR/tests/scenarios/expected/minesweeper/new-game-twice.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/minesweeper/new-game-twice.audit"
cp "$REPO_DIR/tests/scenarios/expected/floppybird/fixed-step-flaps.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/floppybird/fixed-step-flaps.audit"
cp "$REPO_DIR/example/ScrapbookUI/assets/page1.png" "$SANDBOX/snapshot.png"
printf '%s\n' \
  'scrapbook startup' \
  'scrapbook open-first-page-refused' \
  'helloworld startup' \
  'helloworld toggle-action-probe' \
  'tutorial startup' \
  'tutorial increment-summary-toggle' \
  'minesweeper startup' \
  'minesweeper new-game-twice' \
  'floppybird startup' \
  'floppybird fixed-step-flaps' \
  >"$SANDBOX/repo/tests/scenarios/scenarios.txt"
touch \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaHelloWorldTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaTutorialTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaMineSweeperTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaFloppyBirdTestsToolbox68K.bin" \
  "$SANDBOX/repo/example/ScrapbookUI/ASSETS.LRP" \
  "$SANDBOX/BootTemplate.hd"

mkdir -p "$SANDBOX/repo/build/host/lrpc"
cat >"$SANDBOX/repo/build/host/lrpc/lrpc" <<'SH'
#!/usr/bin/env bash
# fake lrpc: stage <source> -o <destination> [--corrupt-bag N]
printf '%s\n' "$@" >"$SANDBOX/lrpc-arguments"
cp -f "$2" "$4"
SH
chmod +x "$SANDBOX/repo/build/host/lrpc/lrpc"

cat >"$SANDBOX/repo/scripts/mame-dev-disk.sh" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$@" >"$SANDBOX/dev-disk-arguments"
: >"$MAME_DEV_HDA"
SH

cat >"$SANDBOX/fake-mame" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "${LOKA_TAB_COUNT-unset}" >"$SANDBOX/tab-count"
printf '%s\n' "${LOKA_SETTLE_TIMEOUT-unset}" >"$SANDBOX/settle-timeout"
if [ "${FAKE_MAME_RESULT:-failure}" = "success" ]; then
  snapshot_directory=""
  while [ "$#" -gt 0 ]; do
    if [ "$1" = "-snapshot_directory" ]; then
      snapshot_directory="$2"
      break
    fi
    shift
  done
  mkdir -p "$snapshot_directory/maciix"
  cp -f "$SANDBOX/snapshot.png" "$snapshot_directory/maciix/0000.png"
  printf 'LOKA-SNAP: settled after 1.00 emulated seconds (3 stable samples)\n' >"$LOKA_SNAP_LOG"
  exit 0
fi
exit 7
SH

cat >"$SANDBOX/retro-tools/hmount" <<'SH'
#!/usr/bin/env bash
exit 0
SH

cat >"$SANDBOX/retro-tools/humount" <<'SH'
#!/usr/bin/env bash
exit 0
SH

cat >"$SANDBOX/retro-tools/hcopy" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
destination="$3"
expected="$SANDBOX/repo/tests/scenarios/expected/${FAKE_EXAMPLE:-scrapbook}/startup.audit"
if [ "${FAKE_AUDIT_MUTATION:-0}" = "1" ]; then
  sed 's/status\tok/status\terror/' "$expected" >"$destination"
else
  cp -f "$expected" "$destination"
fi
SH

chmod +x \
  "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
  "$SANDBOX/repo/scripts/mame-dev-disk.sh" \
  "$SANDBOX/fake-mame" \
  "$SANDBOX/retro-tools/hmount" \
  "$SANDBOX/retro-tools/hcopy" \
  "$SANDBOX/retro-tools/humount"
cat >"$SANDBOX/mame.env" <<EOF
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$SANDBOX/BootTemplate.hd
EOF

run_case() {
  local example="$1"
  local scenario="$2"
  local expected_tab_count="$3"
  local expected_settle_timeout="$4"
  local override="${5:-}"
  local actual_tab_count
  local actual_settle_timeout

  rm -f "$SANDBOX/tab-count" "$SANDBOX/settle-timeout" "$SANDBOX/dev-disk-arguments"
  if [ -n "$override" ]; then
    if MAME_ENV_FILE="$SANDBOX/mame.env" LOKA_TAB_COUNT="$override" \
        env -u WSL_INTEROP \
        bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
          "$example" "$scenario" >"$SANDBOX/runner.log" 2>&1; then
      fail "$example unexpectedly reached a passing MAME verdict"
    fi
  else
    if MAME_ENV_FILE="$SANDBOX/mame.env" \
        env -u WSL_INTEROP -u LOKA_TAB_COUNT \
        bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
          "$example" "$scenario" >"$SANDBOX/runner.log" 2>&1; then
      fail "$example unexpectedly reached a passing MAME verdict"
    fi
  fi

  [ -f "$SANDBOX/tab-count" ] \
    || fail "$example did not launch fake MAME"
  actual_tab_count="$(cat "$SANDBOX/tab-count")"
  [ "$actual_tab_count" = "$expected_tab_count" ] \
    || fail "$example forwarded tab count '$actual_tab_count', expected '$expected_tab_count'"
  actual_settle_timeout="$(cat "$SANDBOX/settle-timeout")"
  [ "$actual_settle_timeout" = "$expected_settle_timeout" ] \
    || fail "$example forwarded settle timeout '$actual_settle_timeout', expected '$expected_settle_timeout'"
}

run_case scrapbook startup 1 unset
run_case scrapbook open-first-page-refused 1 unset
grep -Fxq -- '--corrupt-bag' "$SANDBOX/lrpc-arguments" \
  || fail "Scrapbook refusal did not request package corruption"
grep -Fxq -- '1' "$SANDBOX/lrpc-arguments" \
  || fail "Scrapbook refusal did not use the neutral bag mapping"
run_case helloworld startup 2 unset
run_case helloworld toggle-action-probe 2 unset
run_case tutorial startup 3 unset
run_case tutorial increment-summary-toggle 3 unset
run_case minesweeper startup 4 120
run_case minesweeper new-game-twice 4 120
run_case floppybird startup 4 unset
run_case floppybird fixed-step-flaps 4 unset
run_case helloworld toggle-action-probe 9 unset 9

verify_startup_verdict() {
  local example="$1"
  mkdir -p "$SANDBOX/repo/build/mame-scenario/golden/$example"
  cp "$SANDBOX/snapshot.png" "$SANDBOX/repo/build/mame-scenario/golden/$example/startup.png"
  printf 'maciix\n' \
    >"$SANDBOX/repo/build/mame-scenario/golden/$example/startup.png.mame-machine"
  if ! MAME_ENV_FILE="$SANDBOX/mame.env" \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE="$example" \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" "$example" startup \
        >"$SANDBOX/runner-success.log" 2>&1; then
    fail "$example byte-identical audit and pixel golden did not pass"
  fi

  if MAME_ENV_FILE="$SANDBOX/mame.env" \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE="$example" FAKE_AUDIT_MUTATION=1 \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" "$example" startup \
        >"$SANDBOX/runner-mutation.log" 2>&1; then
    fail "$example observed audit mutation unexpectedly passed"
  fi
  grep -Fq 'audit differs from' "$SANDBOX/runner-mutation.log" \
    || fail "$example observed audit mutation did not fail at the verdict comparison"
}

verify_startup_verdict scrapbook
verify_startup_verdict helloworld
verify_startup_verdict tutorial
verify_startup_verdict minesweeper
verify_startup_verdict floppybird

verify_golden_machine_guard() {
  local golden_dir="$SANDBOX/repo/build/mame-scenario/golden/helloworld"
  local machine_record="$golden_dir/startup.png.mame-machine"

  printf 'maciix\n' >"$machine_record"
  if MAME_ENV_FILE="$SANDBOX/mame.env" MAME_MACHINE=macqd700 \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-machine-mismatch.log" 2>&1; then
    fail "golden machine mismatch unexpectedly passed"
  fi
  grep -Fq "was recorded on MAME machine 'maciix', but the current machine is 'macqd700'" \
    "$SANDBOX/runner-machine-mismatch.log" \
    || fail "golden machine mismatch did not report both machine names"

  rm "$machine_record"
  if MAME_ENV_FILE="$SANDBOX/mame.env" \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-machine-missing.log" 2>&1; then
    fail "golden without a machine record unexpectedly passed"
  fi
  grep -Fq "rerun with --update-golden, or write 'maciix' to $machine_record" \
    "$SANDBOX/runner-machine-missing.log" \
    || fail "missing golden machine record did not explain both recovery paths"

  if ! MAME_ENV_FILE="$SANDBOX/mame.env" MAME_MACHINE=macqd700 \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        --update-golden >"$SANDBOX/runner-machine-update.log" 2>&1; then
    fail "golden update failed while recording its machine"
  fi
  [ "$(cat "$machine_record")" = "macqd700" ] \
    || fail "golden update did not record the current machine"

  if ! MAME_ENV_FILE="$SANDBOX/mame.env" MAME_MACHINE=macqd700 \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-machine-match.log" 2>&1; then
    fail "golden recorded on the current machine did not pass"
  fi
}

verify_golden_machine_guard

echo "Toolbox scenario runner tests passed"
