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
  "$SANDBOX/repo/tests/scenarios" \
  "$SANDBOX/repo/scripts" \
  "$SANDBOX/repo/example/ScrapbookUI" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox"
cp "$RUNNER" "$SANDBOX/repo/tests/toolbox/run-scenario.sh"
cp "$LAUNCHER" "$SANDBOX/repo/tests/toolbox/mame-launch.lua"
printf '%s\n' \
  'scrapbook startup' \
  'helloworld toggle-action-probe' \
  >"$SANDBOX/repo/tests/toolbox/scenarios.txt"
touch \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaTestsToolbox68K.bin" \
  "$SANDBOX/repo/build/retro68/68k/Release/tests/toolbox/LokaHelloWorldTestsToolbox68K.bin" \
  "$SANDBOX/repo/example/ScrapbookUI/ASSETS.LRP" \
  "$SANDBOX/BootTemplate.hd"

mkdir -p "$SANDBOX/repo/build/host/lrpc"
cat >"$SANDBOX/repo/build/host/lrpc/lrpc" <<'SH'
#!/usr/bin/env bash
# fake lrpc: stage <source> -o <destination> [--corrupt-bag N]
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
printf '%s\n' "${LOKA_TAB_COUNT-unset}" >"$SANDBOX/tab-count"
exit 7
SH

chmod +x \
  "$SANDBOX/repo/tests/toolbox/run-scenario.sh" \
  "$SANDBOX/repo/scripts/mame-dev-disk.sh" \
  "$SANDBOX/fake-mame"
cat >"$SANDBOX/mame.env" <<EOF
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$SANDBOX/BootTemplate.hd
EOF

run_case() {
  local example="$1"
  local scenario="$2"
  local expected_tab_count="$3"
  local override="${4:-}"
  local actual_tab_count

  rm -f "$SANDBOX/tab-count" "$SANDBOX/dev-disk-arguments"
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
}

run_case scrapbook startup 1
run_case helloworld toggle-action-probe 2
run_case helloworld toggle-action-probe 9 9

echo "Toolbox scenario runner tests passed"
