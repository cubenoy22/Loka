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
  "$SANDBOX/repo/scripts/rig/toolbox/rigs" \
  "$SANDBOX/repo/example/ScrapbookUI" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox" \
  "$SANDBOX/retro-tools"
cp "$RUNNER" "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
cp "$LAUNCHER" "$SANDBOX/repo/tests/toolbox/mame-launch.lua"
cp "$REPO_DIR/scripts/rig/toolbox/classic_golden_identity.py" \
  "$SANDBOX/repo/scripts/rig/toolbox/classic_golden_identity.py"
cp "$REPO_DIR/scripts/rig/golden_identity_guard.py" \
  "$SANDBOX/repo/scripts/rig/golden_identity_guard.py"
cp "$REPO_DIR/scripts/rig/package_fixture_guard.py" \
  "$SANDBOX/repo/scripts/rig/package_fixture_guard.py"
cp "$REPO_DIR/scripts/retro68-env.sh" \
  "$SANDBOX/repo/scripts/retro68-env.sh"
cp "$REPO_DIR/scripts/env-file.sh" \
  "$SANDBOX/repo/scripts/env-file.sh"
cp "$REPO_DIR/scripts/rig/toolbox/rigs/toolbox-maciix.ini" \
  "$SANDBOX/repo/scripts/rig/toolbox/rigs/toolbox-maciix.ini"
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
cp "$REPO_DIR/example/ScrapbookUI/assets/page2.png" "$SANDBOX/different-snapshot.png"
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
printf '%s\n' \
  'scrapbook open-first-page-refused fake cell, declared so the fixture bundle stages' \
  'helloworld toggle-action-probe fake cell, declared so the fixture bundle stages' \
  'tutorial increment-summary-toggle fake cell, declared so the fixture bundle stages' \
  'minesweeper new-game-twice fake cell, declared so the fixture bundle stages' \
  'floppybird fixed-step-flaps fake cell, declared so the fixture bundle stages' \
  >"$SANDBOX/repo/tests/scenarios/startup-golden-identities.txt"
EMPTY_IDENTITY_DECLARATIONS="$SANDBOX/empty-startup-golden-identities.txt"
: >"$EMPTY_IDENTITY_DECLARATIONS"
touch \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaHelloWorldTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaTutorialTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaMineSweeperTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaFloppyBirdTestsToolbox68K.bin" \
  "$SANDBOX/repo/example/ScrapbookUI/ASSETS.LRP" \
  "$SANDBOX/BootTemplate.hd"
cat >"$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/classic-build-provenance.txt" <<'EOF'
build_provenance_version=1
gcc_version=fake-gcc-12.2.0
universal_interfaces_version=0x0340
retro68_identity_kind=toolchain-content-sha256
retro68_identity=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
EOF

mkdir -p "$SANDBOX/repo/build/host/lrpc"
cat >"$SANDBOX/repo/build/host/lrpc/lrpc" <<'SH'
#!/usr/bin/env bash
# fake lrpc: stage <source> -o <destination> [--corrupt-bag N]
printf '%s\n' "$@" >"$SANDBOX/lrpc-arguments"
cp -f "$2" "$4"
if [ "${5:-}" = "--corrupt-bag" ] && [ "${FAKE_LRPC_IDENTITY:-0}" != "1" ]; then
  printf 'corrupt bag %s\n' "$6" >>"$4"
fi
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
for argument in "$@"; do
  if [ "$argument" = "-verifyroms" ]; then
    printf 'romset maciix is good\n'
    exit 0
  fi
  if [ "$argument" = "-listroms" ]; then
    printf 'ROMs required for driver maciix\nboot.rom  sha1:1111111111111111111111111111111111111111\n'
    exit 0
  fi
done
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
  cp -f "${FAKE_SNAPSHOT:-$SANDBOX/snapshot.png}" "$snapshot_directory/maciix/0000.png"
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
expected="$SANDBOX/repo/tests/scenarios/expected/${FAKE_EXAMPLE:-scrapbook}/${FAKE_SCENARIO:-startup}.audit"
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
printf '/build/\n' >"$SANDBOX/repo/.gitignore"
git -C "$SANDBOX/repo" init -q
git -C "$SANDBOX/repo" config user.name 'Loka Test'
git -C "$SANDBOX/repo" config user.email 'loka-test@example.invalid'
git -C "$SANDBOX/repo" add .
git -C "$SANDBOX/repo" commit -q -m fixture
cat >"$SANDBOX/mame.env" <<EOF
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$SANDBOX/BootTemplate.hd
EOF

verify_preflight_failure_vocabulary() {
  local provenance="$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/classic-build-provenance.txt"
  local saved="$SANDBOX/classic-build-provenance.saved"
  local marker="$SANDBOX/repo/build/mame-scenario/helloworld/startup/machine-verdict.txt"
  local status

  mv "$provenance" "$saved"
  rm -f "$SANDBOX/tab-count"
  set +e
  MAME_ENV_FILE="$SANDBOX/mame.env" env -u WSL_INTEROP -u LOKA_TAB_COUNT \
    bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
      >"$SANDBOX/runner-preflight.log" 2>&1
  status=$?
  set -e
  mv "$saved" "$provenance"

  [ "$status" -eq 3 ] || fail "identity preflight failure did not retain its distinct exit code"
  grep -Fq 'cannot read' "$SANDBOX/runner-preflight.log" \
    || fail "identity preflight failure did not explain the missing provenance"
  grep -Fxq 'machine_verdict=failed-or-not-reached' "$marker" \
    || fail "identity preflight failure recorded the wrong machine verdict"
  grep -Fxq 'runtime_verification=failed-or-not-reached' "$marker" \
    || fail "identity preflight failure recorded the wrong runtime fact"
  if grep -Fq 'machine_verdict=refused' "$marker"; then
    fail "identity preflight failure was misclassified as a reference refusal"
  fi
  [ ! -f "$SANDBOX/tab-count" ] || fail "identity preflight failure launched fake MAME"
}

verify_preflight_failure_vocabulary

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

cp "$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt" \
  "$SANDBOX/fixtures-valid.txt"
printf '%s\n' 'malformed fixture row' \
  >>"$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt"
rm -f "$SANDBOX/tab-count"
if MAME_ENV_FILE="$SANDBOX/mame.env" env -u WSL_INTEROP \
    bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" scrapbook startup \
      >"$SANDBOX/runner-malformed-fixture.log" 2>&1; then
  fail "malformed fixture registry unexpectedly passed"
fi
grep -Fq ':4: invalid package fixture registry entry' \
  "$SANDBOX/runner-malformed-fixture.log" \
  || fail "malformed fixture registry was not rejected with its line number"
[ ! -f "$SANDBOX/tab-count" ] \
  || fail "malformed fixture registry reached MAME launch"
cp "$SANDBOX/fixtures-valid.txt" \
  "$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt"

run_case scrapbook startup 1 unset
run_case scrapbook open-first-page-refused 1 unset
grep -Fxq -- '--corrupt-bag' "$SANDBOX/lrpc-arguments" \
  || fail "Scrapbook refusal did not request package corruption"
grep -A1 -Fx -- '--corrupt-bag' "$SANDBOX/lrpc-arguments" | tail -1 | grep -Fxq -- '1' \
  || fail "Scrapbook refusal did not use the neutral bag mapping"
rm -f "$SANDBOX/tab-count"
if MAME_ENV_FILE="$SANDBOX/mame.env" FAKE_LRPC_IDENTITY=1 env -u WSL_INTEROP \
    bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
      scrapbook open-first-page-refused \
      >"$SANDBOX/runner-unstaged-fixture.log" 2>&1; then
  fail "byte-identical corrupt fixture unexpectedly reached MAME"
fi
grep -Fq "scenario 'open-first-page-refused' declares corrupt-bag=1" \
  "$SANDBOX/runner-unstaged-fixture.log" \
  || fail "package fixture wall did not name the declared corruption"
grep -Fq 'this rail did not stage the fixture' \
  "$SANDBOX/runner-unstaged-fixture.log" \
  || fail "package fixture wall did not diagnose the missing stage"
[ ! -f "$SANDBOX/tab-count" ] \
  || fail "byte-identical corrupt fixture reached MAME launch"
run_case helloworld startup 2 unset
run_case helloworld toggle-action-probe 2 unset
run_case tutorial startup 3 unset
run_case tutorial increment-summary-toggle 3 unset
run_case minesweeper startup 4 120
run_case minesweeper new-game-twice 4 120
run_case floppybird startup 4 unset
run_case floppybird fixed-step-flaps 4 unset
run_case helloworld toggle-action-probe 9 unset 9

IDENTITY_HELPER="$SANDBOX/repo/scripts/rig/toolbox/classic_golden_identity.py"
RIG_DESCRIPTOR="$SANDBOX/repo/scripts/rig/toolbox/rigs/toolbox-maciix.ini"
GOLDEN_BUNDLE="$SANDBOX/repo/build/mame-scenario/golden"
CURRENT_IDENTITY="$SANDBOX/current-identity.txt"
STARTUP_IDENTITY_DECLARATIONS="$SANDBOX/repo/tests/scenarios/startup-golden-identities.txt"

prepare_authorized_bundle() {
  python3 "$IDENTITY_HELPER" capture-current \
    --output "$CURRENT_IDENTITY" \
    --build-provenance "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/classic-build-provenance.txt" \
    --descriptor "$RIG_DESCRIPTOR" \
    --mame-executable "$SANDBOX/fake-mame" \
    --ram-size 8M --machine maciix \
    --capture-adapter mame-screen-snapshot.v1 \
    --boot-hd "$SANDBOX/BootTemplate.hd" \
    || fail "could not capture the fake reference identity"
  while read -r example scenario; do
    python3 "$IDENTITY_HELPER" stage-capture \
      --bundle "$GOLDEN_BUNDLE" \
      --registry "$SANDBOX/repo/tests/scenarios/scenarios.txt" \
      --declarations "$STARTUP_IDENTITY_DECLARATIONS" \
      --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$CURRENT_IDENTITY" \
      --capture "$SANDBOX/snapshot.png" \
      --application "$SANDBOX/fake-mame" \
      --source-tree "$REPO_DIR" \
      --example "$example" --scenario "$scenario" >/dev/null \
      || fail "could not stage fake golden $example/$scenario"
  done <"$SANDBOX/repo/tests/scenarios/scenarios.txt"
  approved="$(sed -n 's/^identity_sha256=//p' "$GOLDEN_BUNDLE/manifest.txt")"
  [ -n "$approved" ] || fail "fake bundle omitted identity_sha256"
  sed -i "s/^reference_identity_sha256 = .*/reference_identity_sha256 = $approved/" \
    "$RIG_DESCRIPTOR"
}

prepare_authorized_bundle

run_golden_update() {
  local example="$1"
  local scenario="$2"
  local log="$3"
  shift 3
  MAME_ENV_FILE="$SANDBOX/mame.env" \
    RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
    FAKE_MAME_RESULT=success FAKE_EXAMPLE="$example" FAKE_SCENARIO="$scenario" \
    env -u WSL_INTEROP -u LOKA_TAB_COUNT "$@" \
    bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
      "$example" "$scenario" --update-golden >"$log" 2>&1
}

verify_record_time_startup_identity_guard() {
  local staging="$GOLDEN_BUNDLE.incomplete"
  local declarations_saved="$SANDBOX/startup-golden-identities.saved"

  cp "$STARTUP_IDENTITY_DECLARATIONS" "$declarations_saved"
  grep -v '^tutorial increment-summary-toggle ' "$declarations_saved" \
    >"$STARTUP_IDENTITY_DECLARATIONS"
  rm -rf "$staging"
  run_golden_update tutorial startup "$SANDBOX/identity-undeclared-startup.log" \
    || fail "could not stage the startup reference for the undeclared case"
  if run_golden_update tutorial increment-summary-toggle \
      "$SANDBOX/identity-undeclared.log"; then
    fail "undeclared startup-identical capture recorded"
  fi
  grep -Fq 'byte-identical to tutorial startup but the identity is not declared' \
    "$SANDBOX/identity-undeclared.log" \
    || fail "undeclared identity refusal did not explain the violated contract"
  [ ! -e "$staging/tutorial/increment-summary-toggle.png" ] \
    || fail "undeclared identity was copied into golden.incomplete before refusal"

  cp "$declarations_saved" "$STARTUP_IDENTITY_DECLARATIONS"
  rm -rf "$staging"
  run_golden_update minesweeper startup "$SANDBOX/identity-declared-startup.log" \
    || fail "could not stage the startup reference for the declared case"
  run_golden_update minesweeper new-game-twice "$SANDBOX/identity-declared.log" \
    || fail "declared startup identity did not record"
  [ -f "$staging/minesweeper/new-game-twice.png" ] \
    || fail "declared identity did not reach golden.incomplete"

  rm -rf "$staging"
  run_golden_update minesweeper startup "$SANDBOX/identity-stale-startup.log" \
    || fail "could not stage the startup reference for the stale case"
  if run_golden_update minesweeper new-game-twice \
      "$SANDBOX/identity-stale.log" \
      FAKE_SNAPSHOT="$SANDBOX/different-snapshot.png"; then
    fail "stale startup-identity declaration recorded different bytes"
  fi
  grep -Fq 'stale startup-identity declaration for minesweeper new-game-twice' \
    "$SANDBOX/identity-stale.log" \
    || fail "stale declaration refusal did not explain the drift"
  [ ! -e "$staging/minesweeper/new-game-twice.png" ] \
    || fail "stale declared identity was copied into golden.incomplete before refusal"
  rm -rf "$staging"
}

verify_record_time_startup_identity_guard

expected_application_hash="$(sha256sum "$SANDBOX/fake-mame" | awk '{print $1}')"
[ "$(grep -c '^application_sha256_' "$GOLDEN_BUNDLE/manifest.txt")" -eq 10 ] \
  || fail "golden manifest did not name every capture producer"
[ "$(grep -c "^application_sha256_[0-9][0-9][0-9][0-9]=$expected_application_hash$" \
    "$GOLDEN_BUNDLE/manifest.txt")" -eq 10 ] \
  || fail "golden manifest recorded the wrong application binary digest"

verify_startup_verdict() {
  local example="$1"
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

assert_refused() {
  local log="$1"
  local marker="$SANDBOX/repo/build/mame-scenario/helloworld/startup/machine-verdict.txt"
  grep -Fq 'machine_verdict=refused' "$log" \
    || fail "identity refusal did not print its machine-readable verdict"
  grep -Fxq 'machine_verdict=refused' "$marker" \
    || fail "identity refusal did not finalize its machine-readable marker"
  grep -Fxq 'runtime_verification=passed' "$marker" \
    || fail "identity refusal erased the successful runtime/audit fact"
}

verify_identity_mismatch_refusal() {
  if MAME_ENV_FILE="$SANDBOX/mame.env" MAME_MACHINE=macqd700 \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-machine-mismatch.log" 2>&1; then
    fail "golden identity mismatch unexpectedly passed"
  fi
  assert_refused "$SANDBOX/runner-machine-mismatch.log"
  grep -Fq "identity mismatch for machine: bundle='maciix', current='macqd700'" \
    "$SANDBOX/runner-machine-mismatch.log" \
    || fail "golden identity mismatch did not name both machine values"
}

verify_identity_mismatch_refusal

verify_strict_manifest_refusals() {
  local manifest="$GOLDEN_BUNDLE/manifest.txt"
  cp "$manifest" "$SANDBOX/manifest.valid"

  printf 'unknown_field=surprise\n' >>"$manifest"
  if MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-manifest-unknown.log" 2>&1; then
    fail "manifest with an unknown field unexpectedly passed"
  fi
  assert_refused "$SANDBOX/runner-manifest-unknown.log"
  grep -Fq 'unknown unknown_field' "$SANDBOX/runner-manifest-unknown.log" \
    || fail "unknown manifest field was not diagnosed"

  cp "$SANDBOX/manifest.valid" "$manifest"
  printf 'machine=maciix\n' >>"$manifest"
  if MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-manifest-duplicate.log" 2>&1; then
    fail "manifest with a duplicate field unexpectedly passed"
  fi
  assert_refused "$SANDBOX/runner-manifest-duplicate.log"
  grep -Fq 'duplicate manifest field machine' "$SANDBOX/runner-manifest-duplicate.log" \
    || fail "duplicate manifest field was not diagnosed"

  grep -v '^ram_size=' "$SANDBOX/manifest.valid" >"$manifest"
  if MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-manifest-missing.log" 2>&1; then
    fail "manifest with a missing field unexpectedly passed"
  fi
  assert_refused "$SANDBOX/runner-manifest-missing.log"
  grep -Fq 'missing ram_size' "$SANDBOX/runner-manifest-missing.log" \
    || fail "missing manifest field was not diagnosed"

  grep -v '^application_sha256_0001=' "$SANDBOX/manifest.valid" >"$manifest"
  if MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-manifest-missing-application.log" 2>&1; then
    fail "manifest without per-capture application provenance unexpectedly passed"
  fi
  assert_refused "$SANDBOX/runner-manifest-missing-application.log"
  grep -Fq 'missing application_sha256_0001' \
    "$SANDBOX/runner-manifest-missing-application.log" \
    || fail "missing per-capture application digest was not diagnosed"
  cp "$SANDBOX/manifest.valid" "$manifest"
}

verify_strict_manifest_refusals

verify_legacy_shape_refusal() {
  mv "$GOLDEN_BUNDLE/manifest.txt" "$SANDBOX/manifest.valid"
  printf 'maciix\n' >"$GOLDEN_BUNDLE/helloworld/startup.png.mame-machine"
  if MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-legacy.log" 2>&1; then
    fail "legacy loose golden unexpectedly produced a pixel verdict"
  fi
  assert_refused "$SANDBOX/runner-legacy.log"
  grep -Fq 're-bake the complete Classic golden bundle with --update-golden' \
    "$SANDBOX/runner-legacy.log" \
    || fail "legacy-shape refusal did not name the required re-bake"
  rm "$GOLDEN_BUNDLE/helloworld/startup.png.mame-machine"
  mv "$SANDBOX/manifest.valid" "$GOLDEN_BUNDLE/manifest.txt"
}

verify_legacy_shape_refusal

verify_atomic_bake_and_no_self_authorize() {
  local bundle="$SANDBOX/atomic-golden"
  local registry="$SANDBOX/atomic-scenarios.txt"
  local current="$SANDBOX/arbitrary-identity.txt"
  printf 'helloworld startup\n' >"$registry"
  cp -a "$GOLDEN_BUNDLE" "$bundle"
  original_manifest_hash="$(sha256sum "$bundle/manifest.txt" | awk '{print $1}')"
  python3 "$IDENTITY_HELPER" capture-current \
    --output "$current" \
    --build-provenance "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/classic-build-provenance.txt" \
    --descriptor "$RIG_DESCRIPTOR" \
    --mame-executable "$SANDBOX/fake-mame" \
    --ram-size 8M --machine macqd700 \
    --capture-adapter mame-screen-snapshot.v1 \
    --boot-hd "$SANDBOX/BootTemplate.hd" \
    || fail "could not capture arbitrary bake identity"
  mkdir "$bundle.previous"
  if python3 "$IDENTITY_HELPER" stage-capture \
      --bundle "$bundle" --registry "$registry" \
      --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
      --application "$SANDBOX/fake-mame" --source-tree "$REPO_DIR" \
      --example helloworld --scenario startup \
      >"$SANDBOX/atomic-failure.log" 2>&1; then
    fail "blocked pre-publication bake unexpectedly passed"
  fi
  [ "$(sha256sum "$bundle/manifest.txt" | awk '{print $1}')" = "$original_manifest_hash" ] \
    || fail "failed bake changed the visible official bundle"
  [ -f "$bundle/manifest.txt" ] \
    || fail "failed bake exposed a partial official bundle"
  rm -rf "$bundle.incomplete" "$bundle.previous"
  python3 "$IDENTITY_HELPER" stage-capture \
    --bundle "$bundle" --registry "$registry" \
    --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
    --application "$SANDBOX/fake-mame" --source-tree "$REPO_DIR" \
    --example helloworld --scenario startup \
    >"$SANDBOX/arbitrary-bake.log" \
    || fail "arbitrary environment could not produce a self-consistent bundle"
  grep -Fq 'Reference eligibility: ineligible' "$SANDBOX/arbitrary-bake.log" \
    || fail "fresh arbitrary bake did not report its ineligibility"
  grep -Fq 'A bake cannot self-authorize' "$SANDBOX/arbitrary-bake.log" \
    || fail "fresh arbitrary bake claimed authority over the tracked digest"
  if python3 "$IDENTITY_HELPER" verify \
      --bundle "$bundle" --registry "$registry" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$current" --example helloworld --scenario startup \
      >"$SANDBOX/arbitrary-verify.log" 2>&1; then
    fail "arbitrary fresh bake was eligible without a tracked digest match"
  fi
}

verify_atomic_bake_and_no_self_authorize

verify_unattestable_retro68_bakes_but_refuses_verdict() {
  local bundle="$SANDBOX/unattestable-golden"
  local registry="$SANDBOX/unattestable-scenarios.txt"
  local provenance="$SANDBOX/unattestable-provenance.txt"
  local current="$SANDBOX/unattestable-identity.txt"
  printf 'helloworld startup\n' >"$registry"
  cat >"$provenance" <<'EOF'
build_provenance_version=1
gcc_version=fake-gcc-12.2.0
universal_interfaces_version=0x0340
retro68_identity_kind=unattestable
retro68_identity=unavailable
EOF
  python3 "$IDENTITY_HELPER" capture-current \
    --output "$current" --build-provenance "$provenance" \
    --descriptor "$RIG_DESCRIPTOR" --mame-executable "$SANDBOX/fake-mame" \
    --ram-size 8M --machine maciix --capture-adapter mame-screen-snapshot.v1 \
    --boot-hd "$SANDBOX/BootTemplate.hd" \
    || fail "unattestable Retro68 provenance prevented current identity capture"
  python3 "$IDENTITY_HELPER" stage-capture \
    --bundle "$bundle" --registry "$registry" \
    --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
    --application "$SANDBOX/fake-mame" --source-tree "$REPO_DIR" \
    --example helloworld --scenario startup >/dev/null \
    || fail "unattestable Retro68 provenance blocked a complete bake"
  if python3 "$IDENTITY_HELPER" verify \
      --bundle "$bundle" --registry "$registry" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$current" --example helloworld --scenario startup \
      >"$SANDBOX/unattestable-verify.log" 2>&1; then
    fail "unattestable Retro68 provenance became pixel-eligible"
  fi
  grep -Fq 'Retro68 identity is unattestable; pixel reference eligibility refused' \
    "$SANDBOX/unattestable-verify.log" \
    || fail "unattestable Retro68 refusal was not explicit"
}

verify_unattestable_retro68_bakes_but_refuses_verdict

verify_bake_source_resume_guards() {
  local registry="$SANDBOX/source-scenarios.txt"
  local source="$SANDBOX/bake-source"
  local no_git="$SANDBOX/no-git-source"
  local bundle="$SANDBOX/source-bound-golden"
  local unattestable_bundle="$SANDBOX/unattestable-source-golden"
  local one_shot_bundle="$SANDBOX/unattestable-one-shot-golden"

  printf 'helloworld startup\nhelloworld toggle-action-probe\n' >"$registry"
  mkdir "$source" "$no_git"
  git -C "$source" init -q
  git -C "$source" config user.name 'Loka Test'
  git -C "$source" config user.email 'loka-test@example.invalid'
  printf 'first\n' >"$source/revision.txt"
  git -C "$source" add revision.txt
  git -C "$source" commit -q -m first

  python3 "$IDENTITY_HELPER" stage-capture \
    --bundle "$bundle" --registry "$registry" \
    --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$CURRENT_IDENTITY" --capture "$SANDBOX/snapshot.png" \
    --application "$SANDBOX/fake-mame" --source-tree "$source" \
    --example helloworld --scenario startup >/dev/null \
    || fail "could not start a source-bound bake"
  printf 'second\n' >"$source/revision.txt"
  if python3 "$IDENTITY_HELPER" stage-capture \
      --bundle "$bundle" --registry "$registry" \
      --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$CURRENT_IDENTITY" --capture "$SANDBOX/snapshot.png" \
      --application "$SANDBOX/fake-mame" --source-tree "$source" \
      --example helloworld --scenario toggle-action-probe \
      >"$SANDBOX/source-resume.log" 2>&1; then
    fail "a differently dirty source tree resumed an existing bake"
  fi
  grep -Fq "incomplete bake source identity differs; remove $bundle.incomplete and restart the bake" \
    "$SANDBOX/source-resume.log" \
    || fail "source mismatch refusal did not give restart instructions"

  python3 "$IDENTITY_HELPER" stage-capture \
    --bundle "$unattestable_bundle" --registry "$registry" \
    --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$CURRENT_IDENTITY" --capture "$SANDBOX/snapshot.png" \
    --application "$SANDBOX/fake-mame" --source-tree "$no_git" \
    --example helloworld --scenario startup >/dev/null \
    || fail "unattestable source could not start a bake"
  grep -Fxq 'bake_source_identity=unattestable' \
    "$unattestable_bundle.incomplete/identity.partial" \
    || fail "unattestable source was not recorded in identity.partial"
  if python3 "$IDENTITY_HELPER" stage-capture \
      --bundle "$unattestable_bundle" --registry "$registry" \
      --declarations "$EMPTY_IDENTITY_DECLARATIONS" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$CURRENT_IDENTITY" --capture "$SANDBOX/snapshot.png" \
      --application "$SANDBOX/fake-mame" --source-tree "$no_git" \
      --example helloworld --scenario toggle-action-probe \
      >"$SANDBOX/unattestable-source-resume.log" 2>&1; then
    fail "an unattestable source resumed an existing bake"
  fi
  grep -Fq "incomplete bake source identity is unattestable; remove $unattestable_bundle.incomplete and restart the bake" \
    "$SANDBOX/unattestable-source-resume.log" \
    || fail "unattestable source refusal did not give restart instructions"

  printf 'helloworld startup\n' >"$SANDBOX/one-shot-scenarios.txt"
  python3 "$IDENTITY_HELPER" stage-capture \
    --bundle "$one_shot_bundle" --registry "$SANDBOX/one-shot-scenarios.txt" \
    --declarations "$EMPTY_IDENTITY_DECLARATIONS" \
    --descriptor "$RIG_DESCRIPTOR" --current-identity "$CURRENT_IDENTITY" \
    --capture "$SANDBOX/snapshot.png" --application "$SANDBOX/fake-mame" \
    --source-tree "$no_git" --example helloworld --scenario startup >/dev/null \
    || fail "unattestable source blocked a from-scratch single-run bake"
  [ -f "$one_shot_bundle/manifest.txt" ] \
    || fail "unattestable one-shot bake did not publish"
  if grep -Fq 'bake_source_identity=' "$one_shot_bundle/manifest.txt"; then
    fail "staging-only source identity leaked into the candidate-matched manifest"
  fi
}

verify_bake_source_resume_guards

verify_provenance_target_is_always_run() {
  local cmake_file="$REPO_DIR/tests/toolbox/CMakeLists.txt"
  local block
  block="$(sed -n '/add_custom_target(LokaToolboxBuildProvenance ALL/,/^)/p' "$cmake_file")"
  printf '%s\n' "$block" | grep -Fq 'COMMAND "${LOKA_TOOLBOX_PYTHON3_EXECUTABLE}"' \
    || fail "build provenance emitter is not owned by the always-run target"
  printf '%s\n' "$block" | grep -Fq 'BYPRODUCTS "${LOKA_TOOLBOX_BUILD_PROVENANCE}"' \
    || fail "always-run provenance target did not declare its output"
}

verify_provenance_target_is_always_run

verify_structural_audit_wording() {
  if ! MAME_ENV_FILE="$SANDBOX/mame.env" RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        --structural-audit >"$SANDBOX/runner-structural.log" 2>&1; then
    fail "structural/audit mode failed"
  fi
  grep -Fq 'Scenario structural/audit pass: helloworld/startup' \
    "$SANDBOX/runner-structural.log" \
    || fail "structural/audit mode omitted its explicit pass wording"
  grep -Fq 'Pixel verdict: not evaluated (Classic structural/audit mode does not claim a pixel verdict)' \
    "$SANDBOX/runner-structural.log" \
    || fail "structural/audit mode implied a pixel verdict"
}

verify_structural_audit_wording

# A pinned boot template is read-only so that booting it in place fails loudly
# instead of silently re-baselining the goldens (#425). cp reproduces that mode,
# so every copy the rail makes for itself arrives read-only unless something
# clears it -- and the emulator writing to the copy is why the copy exists.
verify_writable_boot_copy() {
  local boot="$SANDBOX/repo/build/mame-scenario/helloworld/startup/Boot.hd"

  chmod a-w "$SANDBOX/BootTemplate.hd"
  rm -rf "$SANDBOX/repo/build/mame-scenario/helloworld/startup"
  if ! MAME_ENV_FILE="$SANDBOX/mame.env" MAME_MACHINE=maciix \
      RETRO68_TOOLCHAIN_BIN="$SANDBOX/retro-tools" \
      FAKE_MAME_RESULT=success FAKE_EXAMPLE=helloworld \
      env -u WSL_INTEROP -u LOKA_TAB_COUNT \
      bash "$SANDBOX/repo/tests/toolbox/run-scenario.sh" helloworld startup \
        >"$SANDBOX/runner-readonly-template.log" 2>&1; then
    chmod u+w "$SANDBOX/BootTemplate.hd"
    fail "a read-only boot template stopped the rail"
  fi
  chmod u+w "$SANDBOX/BootTemplate.hd"

  [ -f "$boot" ] || fail "the rail did not leave a boot copy to inspect"
  [ -w "$boot" ] \
    || fail "the boot copy inherited the template's read-only mode"
}

verify_writable_boot_copy

echo "Toolbox scenario runner tests passed"
