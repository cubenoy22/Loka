#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

fail() {
  echo "MacOSScenarioRunnerTest failed: $*" >&2
  exit 1
}

mkdir -p \
  "$SANDBOX/repo/tests/macos" \
  "$SANDBOX/repo/tests/scenarios/expected" \
  "$SANDBOX/repo/build" \
  "$SANDBOX/fake.app/Contents/MacOS"
cp "$REPO_DIR/tests/macos/run-scenario.sh" "$SANDBOX/repo/tests/macos/run-scenario.sh"
cp "$REPO_DIR/tests/macos/validate-work-dir.py" "$SANDBOX/repo/tests/macos/validate-work-dir.py"
mkdir -p "$SANDBOX/repo/scripts/rig"
cp "$REPO_DIR/scripts/rig/golden_identity_guard.py" \
  "$SANDBOX/repo/scripts/rig/golden_identity_guard.py"
cp "$REPO_DIR/tests/scenarios/pngtool.py" "$SANDBOX/repo/tests/scenarios/pngtool.py"
cp "$REPO_DIR/tests/scenarios/scenarios.txt" "$SANDBOX/repo/tests/scenarios/scenarios.txt"
cp "$REPO_DIR/tests/scenarios/startup-golden-identities.txt" \
  "$SANDBOX/repo/tests/scenarios/startup-golden-identities.txt"
for example in scrapbook helloworld tutorial minesweeper floppybird; do
  mkdir -p "$SANDBOX/repo/tests/scenarios/expected/$example"
  cp "$REPO_DIR/tests/scenarios/expected/$example/startup.audit" \
    "$SANDBOX/repo/tests/scenarios/expected/$example/startup.audit"
done
cp "$REPO_DIR/tests/scenarios/expected/helloworld/bmi-roundtrip.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/helloworld/bmi-roundtrip.audit"
cp "$REPO_DIR/tests/scenarios/expected/minesweeper/new-game-twice.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/minesweeper/new-game-twice.audit"

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

for target in \
  LokaHelloWorldScenarioMacOS \
  LokaTutorialScenarioMacOS \
  LokaMineSweeperScenarioMacOS \
  LokaFloppyBirdScenarioMacOS; do
  cp "$SANDBOX/fake.app/Contents/MacOS/LokaScrapbookScenarioMacOS" \
    "$SANDBOX/fake.app/Contents/MacOS/$target"
done

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

echo "macOS scenario runner tests passed"
