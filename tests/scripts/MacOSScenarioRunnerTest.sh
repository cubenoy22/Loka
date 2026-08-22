#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SANDBOX="$(mktemp -d)"
export SANDBOX
trap 'rm -rf "$SANDBOX"' EXIT

fail() {
  echo "MacOSScenarioRunnerTest failed: $*" >&2
  exit 1
}

mkdir -p \
  "$SANDBOX/repo/tests/macos" \
  "$SANDBOX/repo/tests/scenarios/expected" \
  "$SANDBOX/repo/example/ScrapbookUI/assets" \
  "$SANDBOX/repo/build/host/lrpc" \
  "$SANDBOX/repo/build" \
  "$SANDBOX/fake.app/Contents/MacOS" \
  "$SANDBOX/fake.app/Contents/Resources"
cp "$REPO_DIR/tests/macos/run-scenario.sh" "$SANDBOX/repo/tests/macos/run-scenario.sh"
cp "$REPO_DIR/tests/macos/validate-work-dir.py" "$SANDBOX/repo/tests/macos/validate-work-dir.py"
mkdir -p "$SANDBOX/repo/scripts/rig"
cp "$REPO_DIR/scripts/rig/golden_identity_guard.py" \
  "$SANDBOX/repo/scripts/rig/golden_identity_guard.py"
cp "$REPO_DIR/scripts/rig/package_fixture_guard.py" \
  "$SANDBOX/repo/scripts/rig/package_fixture_guard.py"
cp "$REPO_DIR/tests/scenarios/pngtool.py" "$SANDBOX/repo/tests/scenarios/pngtool.py"
cp "$REPO_DIR/tests/scenarios/scenarios.txt" "$SANDBOX/repo/tests/scenarios/scenarios.txt"
cp "$REPO_DIR/tests/scenarios/startup-golden-identities.txt" \
  "$SANDBOX/repo/tests/scenarios/startup-golden-identities.txt"
cp "$REPO_DIR/tests/scenarios/scrapbook-package-fixtures.txt" \
  "$SANDBOX/repo/tests/scenarios/scrapbook-package-fixtures.txt"
cp "$REPO_DIR/example/ScrapbookUI/assets/ASSETS-modern.LRP" \
  "$SANDBOX/repo/example/ScrapbookUI/assets/ASSETS-modern.LRP"
cp "$REPO_DIR/example/ScrapbookUI/assets/ASSETS-modern.LRP" \
  "$SANDBOX/fake.app/Contents/Resources/ASSETS.LRP"
for example in scrapbook helloworld tutorial minesweeper floppybird; do
  mkdir -p "$SANDBOX/repo/tests/scenarios/expected/$example"
  cp "$REPO_DIR/tests/scenarios/expected/$example/startup.audit" \
    "$SANDBOX/repo/tests/scenarios/expected/$example/startup.audit"
done
cp "$REPO_DIR/tests/scenarios/expected/helloworld/bmi-roundtrip.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/helloworld/bmi-roundtrip.audit"
cp "$REPO_DIR/tests/scenarios/expected/minesweeper/new-game-twice.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/minesweeper/new-game-twice.audit"
for scenario in \
  open-first-page-refused \
  refused-flip-keeps-page \
  open-text-page-refused; do
  cp "$REPO_DIR/tests/scenarios/expected/scrapbook/$scenario.audit" \
    "$SANDBOX/repo/tests/scenarios/expected/scrapbook/$scenario.audit"
done

for invocation in \
  'scrapbook startup' \
  'scrapbook flip-forward-back' \
  'helloworld startup' \
  'tutorial startup' \
  'minesweeper startup' \
  'floppybird startup'; do
  grep -Fqx "      - run: tests/macos/run-scenario.sh $invocation --ci-structural" \
    "$REPO_DIR/.github/workflows/macos.yml" \
    || fail "CI does not run the $invocation structural cell with the three-argument protocol"
done
cp "$REPO_DIR/example/ScrapbookUI/assets/page1.png" "$SANDBOX/snapshot.png"
cp "$REPO_DIR/example/ScrapbookUI/assets/page2.png" "$SANDBOX/different-snapshot.png"

cat >"$SANDBOX/fake.app/Contents/MacOS/LokaScrapbookScenarioMacOS" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$0" >"$SANDBOX/launched-binary"
if [ "${FAKE_REQUIRE_STAGED:-0}" = "1" ]; then
  case "$0" in
    "$SANDBOX"/repo/build/macos-scenario/scrapbook/*/stage/*.app/Contents/MacOS/*) ;;
    *) exit 19 ;;
  esac
fi
if [ -f "$(dirname "$0")/../Resources/ASSETS.LRP" ]; then
  printf 'staged-package-present\n' >"$SANDBOX/launched-package"
fi
if [ -n "${FAKE_SAY:-}" ]; then
  printf '%s\n' "$FAKE_SAY"
fi
if [ "${FAKE_STALL:-0}" = "1" ]; then
  exit 0
fi
if [ "${FAKE_AUDIT_MUTATION:-0}" = "1" ]; then
  sed 's/text_empty\ttrue/text_empty\tfalse/' "$FAKE_EXPECTED_AUDIT" >actual.audit
else
  cp -f "$FAKE_EXPECTED_AUDIT" actual.audit
fi
cp -f "$FAKE_SNAPSHOT" actual.png
cp -f "$FAKE_SNAPSHOT" settle-a.png
cp -f "$FAKE_SNAPSHOT" settle-b.png
printf 'fake-profile\n' >actual.profile
printf 'artifacts-ready\n' >complete.tmp
mv -f complete.tmp complete
SH
chmod +x \
  "$SANDBOX/repo/tests/macos/run-scenario.sh" \
  "$SANDBOX/fake.app/Contents/MacOS/LokaScrapbookScenarioMacOS"

cat >"$SANDBOX/repo/build/host/lrpc/lrpc" <<'SH'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$@" >"$SANDBOX/lrpc-arguments"
if [ "${FAKE_LRPC_MODE:-copy}" = "missing" ]; then
  rm -f "$4"
  exit 0
fi
cp -f "$2" "$4"
if [ "${5:-}" = "--corrupt-bag" ] && [ "${FAKE_LRPC_MODE:-copy}" != "identity" ]; then
  printf 'corrupt bag %s\n' "$6" >>"$4"
fi
SH
chmod +x "$SANDBOX/repo/build/host/lrpc/lrpc"

for target in \
  LokaHelloWorldScenarioMacOS \
  LokaTutorialScenarioMacOS \
  LokaMineSweeperScenarioMacOS \
  LokaFloppyBirdScenarioMacOS; do
  cp "$SANDBOX/fake.app/Contents/MacOS/LokaScrapbookScenarioMacOS" \
    "$SANDBOX/fake.app/Contents/MacOS/$target"
done

EXPECTED="$SANDBOX/repo/tests/scenarios/expected/scrapbook/open-first-page-refused.audit"
WORK="$SANDBOX/repo/build/macos-scenario/scrapbook/open-first-page-refused"
if ! LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" FAKE_REQUIRE_STAGED=1 \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      scrapbook open-first-page-refused --ci-structural \
      >"$SANDBOX/runner-refusal-cell.log" 2>&1; then
  fail "Scrapbook refusal cell did not pass with its staged corrupt fixture"
fi
grep -Fxq -- '--corrupt-bag' "$SANDBOX/lrpc-arguments" \
  || fail "Scrapbook refusal cell did not request corruption"
grep -A1 -Fx -- '--corrupt-bag' "$SANDBOX/lrpc-arguments" | tail -1 | grep -Fxq -- '1' \
  || fail "Scrapbook refusal cell did not request corrupt bag 1"
grep -Fq '/stage/LokaScrapbookScenarioMacOS.app/Contents/MacOS/' \
  "$SANDBOX/launched-binary" \
  || fail "Scrapbook refusal cell launched the original bundle"

rm -f "$SANDBOX/launched-binary"
if LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" FAKE_REQUIRE_STAGED=1 FAKE_LRPC_MODE=identity \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      scrapbook open-first-page-refused --ci-structural \
      >"$SANDBOX/runner-unstaged-fixture.log" 2>&1; then
  fail "byte-identical corrupt fixture unexpectedly launched"
fi
grep -Fq "scenario 'open-first-page-refused' declares corrupt-bag=1" \
  "$SANDBOX/runner-unstaged-fixture.log" \
  || fail "package fixture wall did not name the declared corruption"
grep -Fq 'this rail did not stage the fixture' \
  "$SANDBOX/runner-unstaged-fixture.log" \
  || fail "package fixture wall did not diagnose the unchanged fixture"
[ ! -f "$SANDBOX/launched-binary" ] \
  || fail "package fixture wall fired after launch"

# The plan answer is read from a command whose stderr is merged in so that a
# refusal carries its message. On the Win32 rail that command can be
# `wsl.exe python3`, which writes interop warnings to stderr on success, so a
# rail that trusted the merged value would hand the warning to lrpc as a bag
# number. The answer shape is checked instead.
cat >"$SANDBOX/noisy-python3" <<'SH'
#!/usr/bin/env bash
echo "wsl: interop warning simulated" >&2
exec python3 "$@"
SH
chmod +x "$SANDBOX/noisy-python3"
rm -f "$SANDBOX/launched-binary"
if PYTHON3="$SANDBOX/noisy-python3" \
    LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" FAKE_REQUIRE_STAGED=1 \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      scrapbook open-first-page-refused --ci-structural \
      >"$SANDBOX/runner-noisy-plan.log" 2>&1; then
  fail "a plan answer carrying interpreter stderr unexpectedly reached lrpc"
fi
grep -Fq 'not a bag number' "$SANDBOX/runner-noisy-plan.log" \
  || fail "contaminated plan answer was not named as such"
[ ! -f "$SANDBOX/launched-binary" ] \
  || fail "contaminated plan answer reached launch"

for example in scrapbook helloworld tutorial minesweeper floppybird; do
  EXPECTED="$SANDBOX/repo/tests/scenarios/expected/$example/startup.audit"
  WORK="$SANDBOX/repo/build/macos-scenario/$example/startup"
  if ! LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
      LOKA_MACOS_SCENARIO_WORK="$WORK" \
      FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
      bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
        "$example" startup --ci-structural \
        >"$SANDBOX/runner-$example-success.log" 2>&1; then
    fail "$example byte-identical audit did not pass structural mode"
  fi
  if [ "$example" = "scrapbook" ]; then
    if grep -Fxq -- '--corrupt-bag' "$SANDBOX/lrpc-arguments"; then
      fail "non-fixture Scrapbook startup requested package corruption"
    fi
    grep -Fxq -- 'stage' "$SANDBOX/lrpc-arguments" \
      || fail "non-fixture Scrapbook startup did not stage its package"
    grep -Fq '/stage/LokaScrapbookScenarioMacOS.app/Contents/MacOS/' \
      "$SANDBOX/launched-binary" \
      || fail "Scrapbook startup launched the original bundle"
    grep -Fxq 'staged-package-present' "$SANDBOX/launched-package" \
      || fail "staged Scrapbook bundle did not contain the package"
  fi
done

EXPECTED="$SANDBOX/repo/tests/scenarios/expected/scrapbook/startup.audit"
WORK="$SANDBOX/repo/build/macos-scenario/scrapbook/startup"

if LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    FAKE_AUDIT_MUTATION=1 \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      scrapbook startup --ci-structural \
      >"$SANDBOX/runner-mutation.log" 2>&1; then
  fail "observed audit mutation unexpectedly passed"
fi
grep -Fq 'actual.audit differs from' "$SANDBOX/runner-mutation.log" \
  || fail "observed audit mutation did not fail at the verdict comparison"

run_macos_update() {
  local example="$1"
  local scenario="$2"
  local snapshot="$3"
  local expected="$SANDBOX/repo/tests/scenarios/expected/$example/$scenario.audit"
  local work="$SANDBOX/repo/build/macos-scenario/$example/$scenario"
  LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$work" \
    FAKE_EXPECTED_AUDIT="$expected" FAKE_SNAPSHOT="$snapshot" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      "$example" "$scenario" --update-golden
}

run_macos_update helloworld startup "$SANDBOX/snapshot.png" \
  >"$SANDBOX/identity-undeclared-startup.log" 2>&1 \
  || fail "could not record the macOS HelloWorld startup reference"
if run_macos_update helloworld bmi-roundtrip "$SANDBOX/snapshot.png" \
    >"$SANDBOX/identity-undeclared.log" 2>&1; then
  fail "macOS recorded an undeclared startup-identical capture"
fi
grep -Fq 'byte-identical to helloworld startup but the identity is not declared' \
  "$SANDBOX/identity-undeclared.log" \
  || fail "macOS undeclared identity refusal did not name the contract"
[ ! -e "$SANDBOX/repo/build/macos-scenario/golden/helloworld/bmi-roundtrip.png" ] \
  || fail "macOS wrote the undeclared identity before refusal"

run_macos_update minesweeper startup "$SANDBOX/snapshot.png" \
  >"$SANDBOX/identity-declared-startup.log" 2>&1 \
  || fail "could not record the macOS MineSweeper startup reference"
run_macos_update minesweeper new-game-twice "$SANDBOX/snapshot.png" \
  >"$SANDBOX/identity-declared.log" 2>&1 \
  || fail "macOS refused the declared startup identity"
DECLARED_GOLDEN="$SANDBOX/repo/build/macos-scenario/golden/minesweeper/new-game-twice.png"
[ -f "$DECLARED_GOLDEN" ] || fail "macOS did not record the declared identity"
declared_hash="$(sha256sum "$DECLARED_GOLDEN" | awk '{print $1}')"
if run_macos_update minesweeper new-game-twice "$SANDBOX/different-snapshot.png" \
    >"$SANDBOX/identity-stale.log" 2>&1; then
  fail "macOS recorded a stale declared identity"
fi
grep -Fq 'stale startup-identity declaration for minesweeper new-game-twice' \
  "$SANDBOX/identity-stale.log" \
  || fail "macOS stale declaration refusal did not name the drift"
[ "$(sha256sum "$DECLARED_GOLDEN" | awk '{print $1}')" = "$declared_hash" ] \
  || fail "macOS stale declaration refusal overwrote the prior golden"

# A refusal must say how far the app got. #466: a cell timed out with a 0-byte
# runner.log, and the deadline message alone could not tell "hung before it
# wrote anything" from "stopped partway". The stall below is not a hang -- the
# fake app exits early -- but it leaves the same work directory the hang left,
# which is what the census reads.
STALL_WORK="$SANDBOX/repo/build/macos-scenario/tutorial/startup"
if FAKE_STALL=1 \
    LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$STALL_WORK" \
    FAKE_EXPECTED_AUDIT="$SANDBOX/repo/tests/scenarios/expected/tutorial/startup.audit" \
    FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      tutorial startup \
      >"$SANDBOX/stall-silent.log" 2>&1; then
  fail "macOS accepted a run that produced no artifacts"
fi
grep -Fq 'Work state: runner.log 0 bytes' \
  "$SANDBOX/stall-silent.log" \
  || fail "silent stall refusal did not report the empty runner.log"
grep -E '^Absent: .*\bcomplete\b' "$SANDBOX/stall-silent.log" >/dev/null \
  || fail "silent stall refusal did not list the missing completion marker"
grep -E '^Present: .*\bapp\.pid\b' "$SANDBOX/stall-silent.log" >/dev/null \
  || fail "silent stall refusal did not report what the run did produce"

if FAKE_STALL=1 FAKE_SAY='scenario: opened the window' \
    LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$STALL_WORK" \
    FAKE_EXPECTED_AUDIT="$SANDBOX/repo/tests/scenarios/expected/tutorial/startup.audit" \
    FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      tutorial startup \
      >"$SANDBOX/stall-chatty.log" 2>&1; then
  fail "macOS accepted a run that stopped after speaking"
fi
grep -Fq 'last line: scenario: opened the window' "$SANDBOX/stall-chatty.log" \
  || fail "partial stall refusal did not report where the app got to"
if grep -Fq 'runner.log 0 bytes' "$SANDBOX/stall-chatty.log"; then
  fail "partial stall refusal reported an empty runner.log"
fi

# A refusal that fires before the work directory is reset must not describe
# what is in it. Caught on the rig: the build-stage refusal listed the previous
# run's completion marker as if this run had produced it.
if LOKA_MACOS_SCENARIO_APP="$SANDBOX/no-such.app" \
    LOKA_MACOS_SCENARIO_WORK="$STALL_WORK" \
    FAKE_EXPECTED_AUDIT="$SANDBOX/repo/tests/scenarios/expected/tutorial/startup.audit" \
    FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" \
      tutorial startup \
      >"$SANDBOX/stall-preflight.log" 2>&1; then
  fail "macOS accepted a run whose app does not exist"
fi
grep -Fq 'stopped before it reset the work directory' "$SANDBOX/stall-preflight.log" \
  || fail "preflight refusal did not say the work directory is not this run's"
if grep -Eq '^(Present|Absent):' "$SANDBOX/stall-preflight.log"; then
  fail "preflight refusal described a work directory this run never reset"
fi

echo "macOS scenario runner tests passed"
