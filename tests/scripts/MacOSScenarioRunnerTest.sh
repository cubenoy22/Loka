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
  "$SANDBOX/repo/tests/scenarios/expected/scrapbook" \
  "$SANDBOX/repo/build" \
  "$SANDBOX/fake.app/Contents/MacOS"
cp "$REPO_DIR/tests/macos/run-scenario.sh" "$SANDBOX/repo/tests/macos/run-scenario.sh"
cp "$REPO_DIR/tests/macos/validate-work-dir.py" "$SANDBOX/repo/tests/macos/validate-work-dir.py"
cp "$REPO_DIR/tests/scenarios/pngtool.py" "$SANDBOX/repo/tests/scenarios/pngtool.py"
cp "$REPO_DIR/tests/scenarios/expected/scrapbook/startup.audit" \
  "$SANDBOX/repo/tests/scenarios/expected/scrapbook/startup.audit"
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

EXPECTED="$SANDBOX/repo/tests/scenarios/expected/scrapbook/startup.audit"
WORK="$SANDBOX/repo/build/macos-scenario/startup"
if ! LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" startup --ci-structural \
      >"$SANDBOX/runner-success.log" 2>&1; then
  fail "byte-identical audit did not pass structural mode"
fi

if LOKA_MACOS_SCENARIO_APP="$SANDBOX/fake.app" \
    LOKA_MACOS_SCENARIO_WORK="$WORK" \
    FAKE_EXPECTED_AUDIT="$EXPECTED" FAKE_SNAPSHOT="$SANDBOX/snapshot.png" \
    FAKE_AUDIT_MUTATION=1 \
    bash "$SANDBOX/repo/tests/macos/run-scenario.sh" startup --ci-structural \
      >"$SANDBOX/runner-mutation.log" 2>&1; then
  fail "observed audit mutation unexpectedly passed"
fi
grep -Fq 'actual.audit differs from' "$SANDBOX/runner-mutation.log" \
  || fail "observed audit mutation did not fail at the verdict comparison"

echo "macOS scenario runner tests passed"
