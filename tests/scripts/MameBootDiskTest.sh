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
if [ -n "${MAME_BOOT_DISK_FAIL_HCOPY_COUNTER:-}" ]; then
  count=0
  [ -f "$MAME_BOOT_DISK_FAIL_HCOPY_COUNTER" ] && count="$(cat "$MAME_BOOT_DISK_FAIL_HCOPY_COUNTER")"
  count=$((count + 1))
  printf '%s\n' "$count" > "$MAME_BOOT_DISK_FAIL_HCOPY_COUNTER"
  [ "$count" -eq 2 ] && exit 1
fi
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
grep -Fx "hmount <$SANDBOX/single-app.hd.staging>" "$SANDBOX/single-app.log" >/dev/null ||
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

# The all-apps route must keep SmirkyCard's application and editable script
# together. Run from a temporary repository shape because the subject derives
# these inputs from its own project root.
ALL_PROJECT="$SANDBOX/all-project"
mkdir -p "$ALL_PROJECT/scripts" "$ALL_PROJECT/example/ScrapbookUI" "$ALL_PROJECT/example/SmirkyCard"
cp "$SUBJECT" "$ALL_PROJECT/scripts/mame-boot-disk.sh"
cp "$REPO_DIR/scripts/mame-boot-copy.sh" "$ALL_PROJECT/scripts/mame-boot-copy.sh"
cp "$REPO_DIR/scripts/retro68-env.sh" "$ALL_PROJECT/scripts/retro68-env.sh"
cp "$REPO_DIR/scripts/env-file.sh" "$ALL_PROJECT/scripts/env-file.sh"
for app in \
  HelloWorld/LokaHello68K.bin \
  MineSweeper/LokaMine68K.bin \
  SimpleViewer/LokaSimpleViewer68K.bin \
  FloppyBird/LokaFloppyBird68K.bin \
  SmirkBench/LokaSmirkBench68K.bin \
  LazyList/LokaLazyList68K.bin \
  Tutorial/LokaTutorial68K.bin \
  ScrapbookUI/ScrapbookUI68K.bin \
  SmirkyCard/LokaSmirkyCard68K.bin; do
  mkdir -p "$ALL_PROJECT/build/retro68/68k/Release/example/$(dirname "$app")"
  touch "$ALL_PROJECT/build/retro68/68k/Release/example/$app"
done
touch "$ALL_PROJECT/example/ScrapbookUI/ASSETS.LRP" "$ALL_PROJECT/example/SmirkyCard/MAIN.JS"
MAME_BOOT_DISK_TEST_LOG="$SANDBOX/all-apps.log" \
MAME_ENV_FILE="$SANDBOX/missing.env" \
MAME_MACHINE="macplus" \
MAME_HDA="$SANDBOX/boot-template.hd" \
MAME_HOMEPATH="$SANDBOX/home" \
MAME_BOOT_HDA="$SANDBOX/all-apps.hd" \
RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" \
  /bin/bash "$ALL_PROJECT/scripts/mame-boot-disk.sh" --all >/dev/null
grep -Fx "hcopy <-m> <$ALL_PROJECT/build/retro68/68k/Release/example/SmirkyCard/LokaSmirkyCard68K.bin> <:Loka:>" "$SANDBOX/all-apps.log" >/dev/null ||
  fail "all-apps did not copy SmirkyCard"
grep -Fx "hcopy <-r> <$ALL_PROJECT/example/SmirkyCard/MAIN.JS> <:Loka:>" "$SANDBOX/all-apps.log" >/dev/null ||
  fail "all-apps did not copy SmirkyCard MAIN.JS"

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

# A stale source record refreshes the disk before hmount sees the staging copy.
printf 'STALE-DISK' > "$SANDBOX/stale-copy.hd"
printf 'wrong\nsource\n' > "$SANDBOX/stale-copy.hd.source"
run_case stale-copy "$SANDBOX/app.bin"
printf '%s\n%s\n' "$RESOLVED_TEMPLATE" "$TEMPLATE_SHA" | cmp -s - "$SANDBOX/stale-copy.hd.source" || fail "stale source was not refreshed"
grep -Fx "hmount <$SANDBOX/stale-copy.hd.staging>" "$SANDBOX/stale-copy.log" >/dev/null || fail "stale refresh did not mount staging"
cmp -s "$SANDBOX/stale-copy.hd" "$SANDBOX/boot-template.hd" || fail "stale copy was not refreshed before staging"

# Alias refusal happens before chmod or hmount, preserving the template.
ln -s "$SANDBOX/boot-template.hd" "$SANDBOX/alias.hd"
if MAME_BOOT_DISK_TEST_LOG="$SANDBOX/alias.log" MAME_ENV_FILE="$SANDBOX/missing.env" MAME_MACHINE="macplus" MAME_HDA="$SANDBOX/boot-template.hd" MAME_HOMEPATH="$SANDBOX/home" MAME_BOOT_HDA="$SANDBOX/alias.hd" RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" /bin/bash "$SUBJECT" "$SANDBOX/app.bin" >/dev/null 2>&1; then
  fail "alias unexpectedly succeeded"
fi
[ ! -s "$SANDBOX/alias.log" ] || fail "alias reached hmount"
cmp -s "$SANDBOX/boot-template.hd" <(printf 'boot-template\n') || fail "alias changed template"

# A failed second copy discards staging and preserves the prepared disk.
printf 'PREVIOUS-DISK' > "$SANDBOX/partial-failure.hd"
printf '%s\n%s\n' "$RESOLVED_TEMPLATE" "$TEMPLATE_SHA" > "$SANDBOX/partial-failure.hd.source"
cp "$SANDBOX/partial-failure.hd" "$SANDBOX/partial-before"
if MAME_BOOT_DISK_FAIL_HCOPY_COUNTER="$SANDBOX/hcopy-count" MAME_BOOT_DISK_TEST_LOG="$SANDBOX/partial-failure.log" MAME_ENV_FILE="$SANDBOX/missing.env" MAME_MACHINE="macplus" MAME_HDA="$SANDBOX/boot-template.hd" MAME_HOMEPATH="$SANDBOX/home" MAME_BOOT_HDA="$SANDBOX/partial-failure.hd" RETRO68_TOOLCHAIN_BIN="$SANDBOX/bin" /bin/bash "$SUBJECT" "$SANDBOX/app.bin" "$SANDBOX/second-data" >/dev/null 2>&1; then
  fail "partial failure unexpectedly succeeded"
fi
cmp -s "$SANDBOX/partial-before" "$SANDBOX/partial-failure.hd" || fail "partial failure changed boot disk"
[ ! -e "$SANDBOX/partial-failure.hd.staging" ] || fail "partial failure left staging"

# Successful staging publishes after unmount and leaves no staging image.
[ ! -e "$SANDBOX/single-app.hd.staging" ] || fail "successful staging left staging"
cmp -s "$SANDBOX/single-app.hd" "$SANDBOX/boot-template.hd" || fail "successful staging did not publish the image"

printf 'ok: mame-boot-disk supports single-app and plain-data staging\n'
