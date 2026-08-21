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
grep -Fq 'invalid Scrapbook fixture registry line' \
  "$SANDBOX/runner-malformed-fixture.log" \
  || fail "malformed fixture registry was not rejected by the parser"
[ ! -f "$SANDBOX/tab-count" ] \
  || fail "malformed fixture registry reached MAME launch"
cp "$SANDBOX/fixtures-valid.txt" \
  "$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt"

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

IDENTITY_HELPER="$SANDBOX/repo/scripts/rig/toolbox/classic_golden_identity.py"
RIG_DESCRIPTOR="$SANDBOX/repo/scripts/rig/toolbox/rigs/toolbox-maciix.ini"
GOLDEN_BUNDLE="$SANDBOX/repo/build/mame-scenario/golden"
CURRENT_IDENTITY="$SANDBOX/current-identity.txt"

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
      --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$CURRENT_IDENTITY" \
      --capture "$SANDBOX/snapshot.png" \
      --example "$example" --scenario "$scenario" >/dev/null \
      || fail "could not stage fake golden $example/$scenario"
  done <"$SANDBOX/repo/tests/scenarios/scenarios.txt"
  approved="$(sed -n 's/^identity_sha256=//p' "$GOLDEN_BUNDLE/manifest.txt")"
  [ -n "$approved" ] || fail "fake bundle omitted identity_sha256"
  sed -i "s/^reference_identity_sha256 = .*/reference_identity_sha256 = $approved/" \
    "$RIG_DESCRIPTOR"
}

prepare_authorized_bundle

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
      --bundle "$bundle" --registry "$registry" --descriptor "$RIG_DESCRIPTOR" \
      --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
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
    --bundle "$bundle" --registry "$registry" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
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
    --bundle "$bundle" --registry "$registry" --descriptor "$RIG_DESCRIPTOR" \
    --current-identity "$current" --capture "$SANDBOX/snapshot.png" \
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
