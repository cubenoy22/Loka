#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUBJECT="$REPO_DIR/scripts/retro68-cmake.sh"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

fail() {
  echo "Retro68EnvironmentTest failed: $*" >&2
  exit 1
}

[ -x "$SUBJECT" ] || fail "the VS Code wrapper is not executable"

mkdir -p "$SANDBOX/bin" "$SANDBOX/home/tools" "$SANDBOX/home/Retro Build"
cat > "$SANDBOX/bin/cmake" <<'EOF'
#!/usr/bin/env bash
{
  printf 'RETRO68_BUILD_DIR=%s\n' "${RETRO68_BUILD_DIR:-}"
  printf 'RETRO68_TOOLCHAIN_BIN=%s\n' "${RETRO68_TOOLCHAIN_BIN:-}"
  for argument in "$@"; do
    printf 'arg=%s\n' "$argument"
  done
} > "$RETRO68_TEST_CAPTURE"
EOF
cat > "$SANDBOX/home/tools/ninja" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
chmod +x "$SANDBOX/bin/cmake" "$SANDBOX/home/tools/ninja"

printf '%s\r\n' \
  '# host configuration' \
  'RETRO68_BUILD_DIR="$HOME/Retro Build"' \
  'RETRO68_TOOLCHAIN_BIN="${HOME}/tools"' \
  'CMAKE_MAKE_PROGRAM="$HOME/tools/ninja"' \
  > "$SANDBOX/retro68.env"

RETRO68_TEST_CAPTURE="$SANDBOX/configure.capture" \
RETRO68_ENV_FILE="$SANDBOX/retro68.env" \
HOME="$SANDBOX/home" \
PATH="$SANDBOX/bin:/usr/bin:/bin" \
  bash "$SUBJECT" --preset retro68-68k-release

grep -Fxq "RETRO68_BUILD_DIR=$SANDBOX/home/Retro Build" \
  "$SANDBOX/configure.capture" \
  || fail "did not expand HOME in RETRO68_BUILD_DIR"
grep -Fxq "RETRO68_TOOLCHAIN_BIN=$SANDBOX/home/tools" \
  "$SANDBOX/configure.capture" \
  || fail "did not import RETRO68_TOOLCHAIN_BIN"
grep -Fxq "arg=-DCMAKE_MAKE_PROGRAM=$SANDBOX/home/tools/ninja" \
  "$SANDBOX/configure.capture" \
  || fail "did not pass the configured Ninja path to CMake"
grep -Fxq "arg=-DRETRO68_BUILD_DIR=$SANDBOX/home/Retro Build" \
  "$SANDBOX/configure.capture" \
  || fail "did not pass the configured Retro68 build path to CMake"

RETRO68_TEST_CAPTURE="$SANDBOX/caller.capture" \
RETRO68_ENV_FILE="$SANDBOX/retro68.env" \
RETRO68_BUILD_DIR="$SANDBOX/caller-build" \
HOME="$SANDBOX/home" \
PATH="$SANDBOX/bin:/usr/bin:/bin" \
  bash "$SUBJECT" --preset retro68-68k-release
grep -Fxq "RETRO68_BUILD_DIR=$SANDBOX/caller-build" "$SANDBOX/caller.capture" \
  || fail "the local file overrode a caller-provided value"

RETRO68_TEST_CAPTURE="$SANDBOX/cli.capture" \
RETRO68_ENV_FILE="$SANDBOX/retro68.env" \
HOME="$SANDBOX/home" \
PATH="$SANDBOX/bin:/usr/bin:/bin" \
  bash "$SUBJECT" --preset retro68-68k-release \
    "-DRETRO68_BUILD_DIR:PATH=$SANDBOX/cli-build"
[ "$(grep -c '^arg=-DRETRO68_BUILD_DIR' "$SANDBOX/cli.capture")" -eq 1 ] \
  || fail "the wrapper duplicated a typed command-line cache override"
grep -Fxq "arg=-DRETRO68_BUILD_DIR:PATH=$SANDBOX/cli-build" \
  "$SANDBOX/cli.capture" \
  || fail "the wrapper did not preserve a typed command-line cache override"

RETRO68_TEST_CAPTURE="$SANDBOX/build.capture" \
RETRO68_ENV_FILE="$SANDBOX/retro68.env" \
HOME="$SANDBOX/home" \
PATH="$SANDBOX/bin:/usr/bin:/bin" \
  bash "$SUBJECT" --build --preset retro68-68k-release
if grep -q '^arg=-DCMAKE_MAKE_PROGRAM=' "$SANDBOX/build.capture"; then
  fail "the wrapper passed a configure cache option to cmake --build"
fi

printf 'UNRELATED_SETTING=value\n' > "$SANDBOX/invalid.env"
if RETRO68_TEST_CAPTURE="$SANDBOX/invalid.capture" \
    RETRO68_ENV_FILE="$SANDBOX/invalid.env" \
    HOME="$SANDBOX/home" \
    PATH="$SANDBOX/bin:/usr/bin:/bin" \
    bash "$SUBJECT" --preset retro68-68k-release >/dev/null 2>&1; then
  fail "an unsupported setting was accepted"
fi
[ ! -e "$SANDBOX/invalid.capture" ] \
  || fail "CMake ran after the local file was refused"

python3 - "$REPO_DIR/.vscode/tasks.json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as handle:
    tasks = json.load(handle)["tasks"]

wrapper = "${workspaceFolder}/scripts/retro68-cmake.sh"
for task in tasks:
    label = task.get("label", "")
    if label.startswith(("Configure: Retro68", "Build: Retro68")):
        if task.get("command") != wrapper:
            raise SystemExit(f"{label} bypasses {wrapper}")

for label in ("PMonSprite: Configure ScrapbookUI", "PMonSprite: Build ScrapbookUI"):
    task = next(item for item in tasks if item.get("label") == label)
    if "scripts/retro68-cmake.sh" not in task.get("command", ""):
        raise SystemExit(f"{label} bypasses the Retro68 wrapper")
PY

echo "Retro68 environment tests passed"
