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
  mkdir -p "$SANDBOX/repo/tests/toolbox" "$SANDBOX/repo/tests/scenarios"
  cp "$SUBJECT" "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh"
  printf 'first alpha\nsecond beta\n' >"$SANDBOX/repo/tests/scenarios/scenarios.txt"
}

write_success_runner() {
  cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
example="$1"
scenario="$2"
cat >/dev/null
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$example/$scenario"
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
[ -f "$SUCCESS/first/alpha.png" ] || fail "success archive omitted first/alpha.png"
[ -f "$SUCCESS/second/beta.png" ] || fail "success archive omitted second/beta.png"
[ ! -e "$SUCCESS.incomplete" ] || fail "success left an incomplete directory"
grep -q '^presentation_version=1$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest omitted its version"
grep -q '^scenario_count=2$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest scenario count is wrong"
[ "$(grep -c '^capture_sha256=' "$SUCCESS/presentation-manifest.txt")" -eq 2 ] \
  || fail "manifest did not hash both captures"
grep -q '^result=passed$' "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest omitted the passed result"
expected_alpha_hash="$(python3 -c 'import hashlib, pathlib, sys; print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())' "$SUCCESS/first/alpha.png")"
grep -q "^capture_sha256=$expected_alpha_hash  first/alpha.png$" \
  "$SUCCESS/presentation-manifest.txt" \
  || fail "manifest recorded the wrong alpha.png hash"
original_alpha="$(cat "$SUCCESS/first/alpha.png")"
if LOKA_TOOLBOX_PRESENTATION_RUN_ID=success \
    bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null 2>&1; then
  fail "duplicate run ID replaced a completed archive"
fi
[ "$(cat "$SUCCESS/first/alpha.png")" = "$original_alpha" ] \
  || fail "duplicate run ID changed a completed archive"

make_fixture
cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
example="$1"
scenario="$2"
run_id="${LOKA_TOOLBOX_PRESENTATION_RUN_ID:?}"
state="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/test-rail-state"
mkdir -p "$state"
touch "$state/$run_id-entered"
if [ "$run_id" = "parallel-owner" ]; then
  while [ ! -f "$state/release-owner" ]; do sleep 0.02; done
fi
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$example/$scenario"
mkdir -p "$output"
printf 'capture-%s-%s\n' "$run_id" "$scenario" >"$output/$scenario.png"
EOF
chmod +x "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
LOKA_TOOLBOX_PRESENTATION_RUN_ID=parallel-owner \
  bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null &
owner_pid=$!
STATE="$SANDBOX/repo/build/test-rail-state"
deadline=$((SECONDS + 5))
while [ ! -f "$STATE/parallel-owner-entered" ]; do
  if [ "$SECONDS" -ge "$deadline" ]; then
    kill "$owner_pid" 2>/dev/null || true
    wait "$owner_pid" 2>/dev/null || true
    fail "first concurrent rail did not enter its scenario runner"
  fi
  sleep 0.02
done
LOKA_TOOLBOX_PRESENTATION_RUN_ID=parallel-contender \
  bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null &
contender_pid=$!
contender_entered_while_owner_active=0
for _ in {1..50}; do
  if [ -f "$STATE/parallel-contender-entered" ]; then
    contender_entered_while_owner_active=1
    break
  fi
  if ! kill -0 "$contender_pid" 2>/dev/null; then break; fi
  sleep 0.02
done
touch "$STATE/release-owner"
wait "$owner_pid" || fail "first concurrent rail failed"
wait "$contender_pid" || fail "second concurrent rail failed"
[ "$contender_entered_while_owner_active" -eq 0 ] \
  || fail "concurrent presentation rails entered shared scenario work directories"

make_fixture
cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
example="$1"
scenario="$2"
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$example/$scenario"
mkdir -p "$output"
if [ "$scenario" = "beta" ]; then
  printf 'machine_verdict=refused\nruntime_verification=passed\nrefusal_reason=fake mismatch\n' \
    >"$output/machine-verdict.txt"
  exit 7
fi
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
grep -Fxq 'machine_verdict=refused' \
  "$FAILURE_ROOT/runner-failure.incomplete/machine-verdict.txt" \
  || fail "presentation failure did not preserve the machine-readable refusal"

make_fixture
cat >"$SANDBOX/repo/tests/toolbox/run-scenario.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
example="$1"
scenario="$2"
output="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)/build/mame-scenario/$example/$scenario"
mkdir -p "$output"
printf 'machine_verdict=failed-or-not-reached\nruntime_verification=failed-or-not-reached\nrefusal_reason=missing provenance\n' \
  >"$output/machine-verdict.txt"
exit 3
EOF
chmod +x "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
if LOKA_TOOLBOX_PRESENTATION_RUN_ID=preflight-failure \
    bash "$SANDBOX/repo/tests/toolbox/run-presentation-rail.sh" >/dev/null 2>&1; then
  fail "preflight failure reported success"
fi
PREFLIGHT_MARKER="$SANDBOX/repo/build/mame-scenario/presentation/preflight-failure.incomplete/machine-verdict.txt"
grep -Fxq 'machine_verdict=failed-or-not-reached' "$PREFLIGHT_MARKER" \
  || fail "presentation rail did not preserve the preflight machine verdict"
grep -Fxq 'runtime_verification=failed-or-not-reached' "$PREFLIGHT_MARKER" \
  || fail "presentation rail did not preserve the preflight runtime fact"
if grep -Fq 'machine_verdict=refused' "$PREFLIGHT_MARKER"; then
  fail "presentation rail promoted preflight failure to refused"
fi

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
