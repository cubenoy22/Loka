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
cp "$REPO_DIR/tests/scenarios/pngtool.py" "$SANDBOX/repo/tests/scenarios/pngtool.py"
cp "$REPO_DIR/tests/scenarios/scenarios.txt" "$SANDBOX/repo/tests/scenarios/scenarios.txt"
for example in scrapbook helloworld tutorial minesweeper floppybird; do
  mkdir -p "$SANDBOX/repo/tests/scenarios/expected/$example"
  cp "$REPO_DIR/tests/scenarios/expected/$example/startup.audit" \
    "$SANDBOX/repo/tests/scenarios/expected/$example/startup.audit"
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

echo "macOS scenario runner tests passed"
