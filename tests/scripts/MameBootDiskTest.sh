#!/usr/bin/env bash

set -euo pipefail

TEST_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$TEST_SCRIPT_DIR/../.." && pwd)"
SUBJECT="${LOKA_MAME_BOOT_DISK_SH:-$REPO_DIR/scripts/mame-boot-disk.sh}"

SANDBOX="$(mktemp -d)"

cleanup() {
  rm -rf "$SANDBOX"
}
trap cleanup EXIT

mkdir -p "$SANDBOX/bin" "$SANDBOX/home"
touch "$SANDBOX/app.bin" "$SANDBOX/first data" "$SANDBOX/second-data"
printf 'boot-template\n' > "$SANDBOX/boot-template.hd"

cat > "$SANDBOX/bin/hmount" <<'EOF'
#!/usr/bin/env bash
printf 'hmount <%s>\n' "$@" >> "$MAME_BOOT_DISK_TEST_LOG"
EOF

cat > "$SANDBOX/bin/hcopy" <<'EOF'
#!/usr/bin/env bash
printf 'hcopy' >> "$MAME_BOOT_DISK_TEST_LOG"
printf ' <%s>' "$@" >> "$MAME_BOOT_DISK_TEST_LOG"
printf '\n' >> "$MAME_BOOT_DISK_TEST_LOG"
EOF

cat > "$SANDBOX/bin/hls" <<'EOF'
#!/usr/bin/env bash
printf 'hls <%s>\n' "$@" >> "$MAME_BOOT_DISK_TEST_LOG"
exit 0
EOF

cat > "$SANDBOX/bin/hmkdir" <<'EOF'
#!/usr/bin/env bash
printf 'hmkdir <%s>\n' "$@" >> "$MAME_BOOT_DISK_TEST_LOG"
EOF

cat > "$SANDBOX/bin/humount" <<'EOF'
#!/usr/bin/env bash
printf 'humount\n' >> "$MAME_BOOT_DISK_TEST_LOG"
EOF

chmod +x "$SANDBOX/bin/hmount" "$SANDBOX/bin/hcopy" "$SANDBOX/bin/hls" "$SANDBOX/bin/hmkdir" "$SANDBOX/bin/humount"

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  exit 1
}

run_case() {
  local case_name="$1"
  shift

  MAME_BOOT_DISK_TEST_LOG="$SANDBOX/$case_name.log" \
  MAME_ENV_FILE="$SANDBOX/missing.env" \
  MAME_MACHINE="macplus" \
  MAME_HDA="$SANDBOX/boot-template.hd" \
  MAME_HOMEPATH="$SANDBOX/home" \
  MAME_BOOT_HDA="$SANDBOX/$case_name.hd" \
  RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" \
    /bin/bash "$SUBJECT" "$@" >/dev/null

  [ -f "$SANDBOX/$case_name.hd" ] ||
    fail "$case_name did not produce or retain the boot disk"
}

run_case single-app "$SANDBOX/app.bin"
grep -Fx "hmount <$SANDBOX/single-app.hd>" "$SANDBOX/single-app.log" >/dev/null ||
  fail "single-app hmount failed"
grep -Fx "hcopy <-m> <$SANDBOX/app.bin> <:Loka:>" "$SANDBOX/single-app.log" >/dev/null ||
  fail "single-app hcopy failed"
grep -Fx "humount" "$SANDBOX/single-app.log" >/dev/null ||
  fail "single-app humount failed"

run_case with-data "$SANDBOX/app.bin" "$SANDBOX/first data" "$SANDBOX/second-data"
grep -Fx "hcopy <-m> <$SANDBOX/app.bin> <:Loka:>" "$SANDBOX/with-data.log" >/dev/null ||
  fail "with-data app copy changed"
grep -Fx "hcopy <-r> <$SANDBOX/first data> <:Loka:>" "$SANDBOX/with-data.log" >/dev/null ||
  fail "spaced data path was not preserved"
grep -Fx "hcopy <-r> <$SANDBOX/second-data> <:Loka:>" "$SANDBOX/with-data.log" >/dev/null ||
  fail "second data path was not copied"

# A fresh boot copy must carry the same .source record mame-run.sh writes
# (resolved template path + sha256), or the next launch refreshes the copy
# and drops the staged applications. Reach the template through a symlink
# and a relative path to prove the resolution.
mkdir -p "$SANDBOX/links"
ln -s "$SANDBOX/boot-template.hd" "$SANDBOX/links/template-link.hd"
( cd "$SANDBOX/links" && \
  MAME_BOOT_DISK_TEST_LOG="$SANDBOX/fresh-copy.log" \
  MAME_ENV_FILE="$SANDBOX/missing.env" \
  MAME_MACHINE="macplus" \
  MAME_HDA="./template-link.hd" \
  MAME_HOMEPATH="$SANDBOX/home" \
  MAME_BOOT_HDA="$SANDBOX/fresh-copy.hd" \
  RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" \
    /bin/bash "$SUBJECT" "$SANDBOX/app.bin" >/dev/null )
[ -f "$SANDBOX/fresh-copy.hd" ] || fail "fresh copy was not created from the template"
cmp -s "$SANDBOX/fresh-copy.hd" "$SANDBOX/boot-template.hd" || fail "fresh copy differs from the template"
RESOLVED_TEMPLATE="$(cd "$SANDBOX" && pwd -P)/boot-template.hd"
TEMPLATE_SHA="$(sha256sum < "$SANDBOX/boot-template.hd" | cut -d' ' -f1)"
printf '%s\n%s\n' "$RESOLVED_TEMPLATE" "$TEMPLATE_SHA" | cmp -s - "$SANDBOX/fresh-copy.hd.source" ||
  fail ".source record does not match mame-run.sh's resolved-path format: $(cat "$SANDBOX/fresh-copy.hd.source")"

printf 'ok: mame-boot-disk supports single-app and plain-data staging\n'
