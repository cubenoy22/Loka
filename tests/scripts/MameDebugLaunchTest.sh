#!/usr/bin/env bash

set -euo pipefail

TEST_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$TEST_SCRIPT_DIR/../.." && pwd)"
SUBJECT="${LOKA_MAME_DEBUG_SH:-$REPO_DIR/scripts/mame-debug.sh}"

SANDBOX="$(mktemp -d)"
export SANDBOX

cleanup() {
  if [ -f "$SANDBOX/mame.pid" ]; then
    mame_pid="$(cat "$SANDBOX/mame.pid")"
    if kill -0 "$mame_pid" 2>/dev/null; then
      kill "$mame_pid" 2>/dev/null || true
      /bin/sleep 0.05
      kill -9 "$mame_pid" 2>/dev/null || true
    fi
  fi
  rm -rf "$SANDBOX"
}
trap cleanup EXIT

mkdir -p "$SANDBOX/scripts" "$SANDBOX/bin"
cp "$SUBJECT" "$SANDBOX/scripts/mame-debug.sh"
touch "$SANDBOX/scripts/mame-find-base.lua" "$SANDBOX/scripts/mame-attach.gdb"

cat >"$SANDBOX/scripts/mame-dev-disk.sh" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$SANDBOX/devdisk.argv"
: > "$MAME_DEV_HDA"
EOF

cat >"$SANDBOX/bin/gdb-multiarch" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$SANDBOX/gdb.argv"
exit "${FAKE_GDB_EXIT:-0}"
EOF

cat >"$SANDBOX/bin/ss" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' 'LISTEN 0 1 127.0.0.1:12399 0.0.0.0:*'
EOF

cat >"$SANDBOX/bin/sleep" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF

cat >"$SANDBOX/fake-mame" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$$" > "$SANDBOX/mame.pid"
exec /bin/sleep 300
EOF

chmod +x \
  "$SANDBOX/scripts/mame-debug.sh" \
  "$SANDBOX/scripts/mame-dev-disk.sh" \
  "$SANDBOX/bin/gdb-multiarch" \
  "$SANDBOX/bin/ss" \
  "$SANDBOX/bin/sleep" \
  "$SANDBOX/fake-mame"

touch "$SANDBOX/dummy.hda" "$SANDBOX/app.bin"
cat >"$SANDBOX/mame.env" <<EOF
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$SANDBOX/dummy.hda
MAME_DEBUG_PORT=12399
EOF

export PATH="$SANDBOX/bin:$PATH"

fail() {
  printf 'FAIL: %s %s\n' "$1" "$2"
  exit 1
}

pass() {
  printf 'ok: %s\n' "$1"
}

assert_argv() {
  local case_name="$1"
  local actual="$2"
  local expected="$SANDBOX/$case_name.expected.argv"
  shift 2

  printf '%s\n' "$@" > "$expected"
  diff -u "$expected" "$actual" ||
    fail "$case_name" "argv did not match"
}

assert_mame_dead() {
  local case_name="$1"
  local mame_pid
  local attempt

  [ -f "$SANDBOX/mame.pid" ] ||
    fail "$case_name" "fake MAME did not record its PID"
  mame_pid="$(cat "$SANDBOX/mame.pid")"

  for attempt in $(seq 40); do
    if ! kill -0 "$mame_pid" 2>/dev/null; then
      return
    fi
    /bin/sleep 0.05
  done

  fail "$case_name" "fake MAME PID $mame_pid is still alive"
}

run_case() {
  local case_name="$1"
  local gdb_exit="$2"
  local expected_exit="$3"
  local with_elf="$4"
  local actual_exit
  shift 4

  rm -f \
    "$SANDBOX/mame.pid" \
    "$SANDBOX/app.code.bin.gdb" \
    "$SANDBOX/devdisk.argv" \
    "$SANDBOX/gdb.argv"
  if [ "$with_elf" = "yes" ]; then
    touch "$SANDBOX/app.code.bin.gdb"
  fi

  if FAKE_GDB_EXIT="$gdb_exit" MAME_ENV_FILE="$SANDBOX/mame.env" \
      env -u WSL_INTEROP -u LOKA_DEV_DATA -u LOKA_GDB_SCRIPT "$@" \
        bash "$SANDBOX/scripts/mame-debug.sh" \
        attach "$SANDBOX/app.bin" 4feffefc 48e71e30 00029372 0x00700000 \
        >"$SANDBOX/$case_name.log" 2>&1; then
    actual_exit=0
  else
    actual_exit=$?
  fi

  [ "$actual_exit" -eq "$expected_exit" ] ||
    fail "$case_name" "expected exit $expected_exit, got $actual_exit"
  assert_mame_dead "$case_name"
}

run_case reap-on-quit 0 0 yes
assert_argv \
  reap-on-quit \
  "$SANDBOX/gdb.argv" \
  -q \
  -x \
  "$SANDBOX/scripts/mame-attach.gdb"
pass reap-on-quit

run_case reap-on-gdb-failure 3 3 yes
pass reap-on-gdb-failure
run_case reap-on-missing-elf 0 1 no
pass reap-on-missing-elf

run_case \
  data-files-arrive-intact \
  0 \
  0 \
  yes \
  "LOKA_DEV_DATA=$SANDBOX/data one.lrp
$SANDBOX/glob*2.lrp"
assert_argv \
  data-files-arrive-intact \
  "$SANDBOX/devdisk.argv" \
  "$SANDBOX/app.bin" \
  "$SANDBOX/data one.lrp" \
  "$SANDBOX/glob*2.lrp"
pass data-files-arrive-intact

touch "$SANDBOX/extra cmds.gdb"
run_case \
  gdb-script-appended \
  0 \
  0 \
  yes \
  "LOKA_GDB_SCRIPT=$SANDBOX/extra cmds.gdb"
assert_argv \
  gdb-script-appended \
  "$SANDBOX/gdb.argv" \
  -q \
  -x \
  "$SANDBOX/scripts/mame-attach.gdb" \
  -x \
  "$SANDBOX/extra cmds.gdb"
pass gdb-script-appended
