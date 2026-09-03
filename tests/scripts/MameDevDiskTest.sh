#!/usr/bin/env bash

set -euo pipefail

TEST_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$TEST_SCRIPT_DIR/../.." && pwd)"
SUBJECT="${LOKA_MAME_DEV_DISK_SH:-$REPO_DIR/scripts/mame-dev-disk.sh}"

SANDBOX="$(mktemp -d)"

cleanup() {
  rm -rf "$SANDBOX"
}
trap cleanup EXIT

mkdir -p "$SANDBOX/bin" "$SANDBOX/home"
touch "$SANDBOX/app.bin" "$SANDBOX/first data" "$SANDBOX/second-data"
printf 'boot-template\n' > "$SANDBOX/boot.hd"

cat > "$SANDBOX/bin/hformat" <<'EOF'
#!/usr/bin/env bash
printf 'hformat' >> "$MAME_DEV_DISK_TEST_LOG"
printf ' <%s>' "$@" >> "$MAME_DEV_DISK_TEST_LOG"
printf '\n' >> "$MAME_DEV_DISK_TEST_LOG"
EOF

cat > "$SANDBOX/bin/hcopy" <<'EOF'
#!/usr/bin/env bash
printf 'hcopy' >> "$MAME_DEV_DISK_TEST_LOG"
printf ' <%s>' "$@" >> "$MAME_DEV_DISK_TEST_LOG"
printf '\n' >> "$MAME_DEV_DISK_TEST_LOG"
EOF

cat > "$SANDBOX/bin/humount" <<'EOF'
#!/usr/bin/env bash
printf 'humount\n' >> "$MAME_DEV_DISK_TEST_LOG"
EOF

chmod +x "$SANDBOX/bin/hformat" "$SANDBOX/bin/hcopy" "$SANDBOX/bin/humount"

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  exit 1
}

run_case() {
  local case_name="$1"
  shift

  MAME_DEV_DISK_TEST_LOG="$SANDBOX/$case_name.log" \
  MAME_ENV_FILE="$SANDBOX/missing.env" \
  MAME_HDA="$SANDBOX/boot.hd" \
  MAME_HOMEPATH="$SANDBOX/home" \
  MAME_DEV_HDA="$SANDBOX/$case_name.hd" \
  RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" \
    /bin/bash "$SUBJECT" "$SANDBOX/app.bin" "$@" >/dev/null

  [ -f "$SANDBOX/$case_name.hd" ] ||
    fail "$case_name did not produce the development disk"
}

# macOS ships Bash 3.2, where expanding an empty array under set -u fails.
# The no-data form is the one used by SimpleViewer's VS Code task.
run_case no-data
# The temporary suffix is the script process ID, so compare the stable calls
# separately and only require the hformat target to have the expected prefix.
grep -F "hformat <-l> <LokaDev> <$SANDBOX/no-data.hd." \
  "$SANDBOX/no-data.log" >/dev/null || fail "no-data hformat arguments changed"
grep -Fx "hcopy <-m> <$SANDBOX/app.bin> <:>" \
  "$SANDBOX/no-data.log" >/dev/null || fail "no-data app copy changed"
[ "$(grep -c '^hcopy ' "$SANDBOX/no-data.log")" -eq 1 ] ||
  fail "no-data case copied an unexpected plain-data file"

run_case with-data "$SANDBOX/first data" "$SANDBOX/second-data"
grep -Fx "hcopy <-r> <$SANDBOX/first data> <:>" \
  "$SANDBOX/with-data.log" >/dev/null || fail "spaced data path was not preserved"
grep -Fx "hcopy <-r> <$SANDBOX/second-data> <:>" \
  "$SANDBOX/with-data.log" >/dev/null || fail "second data path was not copied"
[ "$(grep -c '^hcopy ' "$SANDBOX/with-data.log")" -eq 3 ] ||
  fail "with-data case made an unexpected number of copies"

printf 'ok: mame-dev-disk supports zero or more plain-data files\n'
