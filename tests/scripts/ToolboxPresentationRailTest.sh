#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUBJECT="$REPO_DIR/tests/toolbox/run-presentation-rail.sh"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

fail() {
  echo "ToolboxPresentationRailTest failed: $*" >&2
  exit 1
}

make_fixture() {
  rm -rf "$SANDBOX/repo"
  mkdir -p "$SANDBOX/repo/tests/toolbox"
  cp "$SUBJECT" "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh"
  printf 'alpha\nbeta\n' >"$SANDBOX/repo/tests/toolbox/scrapbook-scenarios.txt"
}

write_success_runner() {
  cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
scenario="$1"
cat >/dev/null
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$scenario"
mkdir -p "$output"
printf 'capture-%s\n' "$scenario" >"$output/$scenario.png"
EOF
  chmod +x "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
}

make_fixture
write_success_runner
LOKA_TOOLBOX_PRESENTATION_RUN_ID=success \
  bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null
SUCCESS="$SANDBOX/repo/build/mame-scenario/presentation/success"
[ -f "$SUCCESS/alpha.png" ] || fail "success archive omitted alpha.png"
[ -f "$SUCCESS/beta.png" ] || fail "success archive omitted beta.png"
[ ! -e "$SUCCESS.incomplete" ] || fail "success left an incomplete directory"
grep -q '^presentation_version=1$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest omitted its version"
grep -q '^scenario_count=2$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest scenario count is wrong"
[ "$(grep -c '^capture_sha256=' "$SUCCESS/presentation-manifest.txt")" -eq 2 ] \
  || fail "manifest did not hash both captures"
grep -q '^result=passed$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest omitted the passed result"
expected_alpha_hash="$(python3 -c 'import hashlib, pathlib, sys; print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())' "$SUCCESS/alpha.png")"
grep -q "^capture_sha256=$expected_alpha_hash  alpha.png$" \
  "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest recorded the wrong alpha.png hash"
original_alpha="$(cat "$SUCCESS/alpha.png")"
if LOKA_TOOLBOX_PRESENTATION_RUN_ID=success \
    bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null 2>&1; then
  fail "duplicate run ID replaced a completed archive"
fi
[ "$(cat "$SUCCESS/alpha.png")" = "$original_alpha" ] \
  || fail "duplicate run ID changed a completed archive"

make_fixture
cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
scenario="$1"
if [ "$scenario" = "beta" ]; then exit 7; fi
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$scenario"
mkdir -p "$output"
printf 'capture-%s\n' "$scenario" >"$output/$scenario.png"
EOF
chmod +x "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
if LOKA_TOOLBOX_PRESENTATION_RUN_ID=runner-failure \
    bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null 2>&1; then
  fail "scenario failure reported success"
fi
FAILURE_ROOT="$SANDBOX/repo/build/mame-scenario/presentation"
[ -d "$FAILURE_ROOT/runner-failure.incomplete" ] \
  || fail "scenario failure did not preserve the incomplete directory"
[ ! -e "$FAILURE_ROOT/runner-failure" ] \
  || fail "scenario failure published a completed directory"

make_fixture
cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
if LOKA_TOOLBOX_PRESENTATION_RUN_ID=missing-capture \
    bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null 2>&1; then
  fail "missing capture reported success"
fi
FAILURE_ROOT="$SANDBOX/repo/build/mame-scenario/presentation"
[ -d "$FAILURE_ROOT/missing-capture.incomplete" ] \
  || fail "missing capture did not preserve the incomplete directory"
[ ! -e "$FAILURE_ROOT/missing-capture" ] \
  || fail "missing capture published a completed directory"

echo "Toolbox presentation rail tests passed"
