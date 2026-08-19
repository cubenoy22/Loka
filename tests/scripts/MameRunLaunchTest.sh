#!/usr/bin/env bash

# MAME writes back to whatever it boots. The configured MAME_HDA is the Classic
# rail's template -- the System fonts and control chrome the pixel goldens are
# made of live inside it -- so the launcher must boot a copy and leave the
# template alone. These cases pin that discipline; the launcher passed MAME_HDA
# straight to -hard1 until #425.

set -euo pipefail

TEST_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$TEST_SCRIPT_DIR/../.." && pwd)"
SUBJECT="${LOKA_MAME_RUN_SH:-$REPO_DIR/scripts/mame-run.sh}"
SUBJECT_PS1="${LOKA_MAME_RUN_PS1:-$REPO_DIR/scripts/mame-run.ps1}"

SANDBOX="$(mktemp -d)"
export SANDBOX

cleanup() {
  rm -rf "$SANDBOX"
}
trap cleanup EXIT

mkdir -p "$SANDBOX/scripts" "$SANDBOX/build"
cp "$SUBJECT" "$SANDBOX/scripts/mame-run.sh"
touch "$SANDBOX/scripts/mame-floppy-service.lua"
chmod +x "$SANDBOX/scripts/mame-run.sh"

# Stands in for the emulator: records its argv, then writes to the disk it was
# handed. Writing is the point -- a launcher that hands over the template would
# corrupt it here exactly as MAME does in the field.
cat >"$SANDBOX/fake-mame" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$@" > "$SANDBOX/mame.argv"
boot=""
previous=""
for argument in "$@"; do
  if [ "$previous" = "-hard1" ]; then
    boot="$argument"
  fi
  previous="$argument"
done
if [ -n "$boot" ]; then
  printf 'WRITTEN-BY-EMULATOR' > "$boot"
fi
EOF
chmod +x "$SANDBOX/fake-mame"

TEMPLATE="$SANDBOX/template.hda"
printf 'PRISTINE-TEMPLATE-BYTES' > "$TEMPLATE"
TEMPLATE_DIGEST="$(sha256sum "$TEMPLATE" | cut -d' ' -f1)"

cat >"$SANDBOX/mame.env" <<EOF
MAME_MACHINE=maciix
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$TEMPLATE
MAME_HOMEPATH=$SANDBOX/mame-home
EOF

fail() {
  printf 'FAIL: %s %s\n' "$1" "$2"
  exit 1
}

pass() {
  printf 'ok: %s\n' "$1"
}

# The boot disk travels as an adjacent "-hard1 <value>" pair in the argv.
hard1_value() {
  grep -A1 -Fx -- '-hard1' "$SANDBOX/mame.argv" | tail -1
}

assert_template_pristine() {
  local case_name="$1"
  local digest
  digest="$(sha256sum "$TEMPLATE" | cut -d' ' -f1)"
  [ "$digest" = "$TEMPLATE_DIGEST" ] ||
    fail "$case_name" "the emulator was allowed to write to the template"
}

# The launcher delegates to the PowerShell twin under WSL, so the shell path is
# only reachable with the interop marker cleared.
run_launcher() {
  env -u WSL_INTEROP MAME_ENV_FILE="$SANDBOX/mame.env" "$@" \
    bash "$SANDBOX/scripts/mame-run.sh"
}

BOOT_COPY="$SANDBOX/build/mame-run/Boot.hd"

# --- the emulator receives a copy, and the template survives the write --------
rm -rf "$SANDBOX/build/mame-run"
run_launcher >"$SANDBOX/boots-a-copy.log" 2>&1 ||
  fail boots-a-copy "the launcher exited non-zero"
[ -f "$SANDBOX/mame.argv" ] ||
  fail boots-a-copy "fake MAME did not record its argv"
[ "$(hard1_value)" = "$BOOT_COPY" ] ||
  fail boots-a-copy "expected -hard1 $BOOT_COPY, got $(hard1_value)"
[ "$(hard1_value)" != "$TEMPLATE" ] ||
  fail boots-a-copy "the template itself was handed to the emulator"
assert_template_pristine boots-a-copy
pass boots-a-copy

# --- the copy is not remade, so an interactive session keeps its state --------
printf 'SESSION-STATE' > "$BOOT_COPY"
run_launcher >"$SANDBOX/copy-persists.log" 2>&1 ||
  fail copy-persists "the launcher exited non-zero"
grep -Fxq 'WRITTEN-BY-EMULATOR' "$BOOT_COPY" ||
  fail copy-persists "the emulator did not write to the persisted copy"
assert_template_pristine copy-persists
pass copy-persists

# --- MAME_BOOT_HDA relocates the copy ----------------------------------------
rm -rf "$SANDBOX/build/mame-run"
ELSEWHERE="$SANDBOX/elsewhere/Boot.hd"
run_launcher "MAME_BOOT_HDA=$ELSEWHERE" >"$SANDBOX/override.log" 2>&1 ||
  fail boot-copy-override "the launcher exited non-zero"
[ "$(hard1_value)" = "$ELSEWHERE" ] ||
  fail boot-copy-override "expected -hard1 $ELSEWHERE, got $(hard1_value)"
assert_template_pristine boot-copy-override
pass boot-copy-override

# --- the copy is whole, and no partial is left behind ------------------------
# The copy goes through a temporary and is renamed, so an interrupted run cannot
# leave a truncated image that every later run would find, skip the copy for,
# and boot.
rm -rf "$SANDBOX/build/mame-run"
run_launcher >"$SANDBOX/copy-is-complete.log" 2>&1 ||
  fail copy-is-complete "the launcher exited non-zero"
[ -z "$(find "$SANDBOX/build/mame-run" -name '*.partial' -print -quit)" ] ||
  fail copy-is-complete "a partial copy was left behind"
# The emulator has since written to the copy, so compare what it received
# against the template by size: a truncated copy is the failure being pinned.
[ "$(wc -c < "$BOOT_COPY")" -gt 0 ] ||
  fail copy-is-complete "the boot copy is empty"
assert_template_pristine copy-is-complete
pass copy-is-complete

# --- a boot copy that aliases the template is refused ------------------------
# Without this the existence check passes and -hard1 names the template after
# all, which is exactly the failure this whole change removes.
assert_alias_refused() {
  local case_name="$1"
  local alias_path="$2"
  rm -f "$SANDBOX/mame.argv"
  if run_launcher "MAME_BOOT_HDA=$alias_path" \
      >"$SANDBOX/$case_name.log" 2>&1; then
    fail "$case_name" "an aliased boot copy was accepted"
  fi
  grep -q "resolves to the boot template itself" "$SANDBOX/$case_name.log" ||
    fail "$case_name" "the refusal did not explain itself"
  [ ! -f "$SANDBOX/mame.argv" ] ||
    fail "$case_name" "the emulator was launched despite the refusal"
  assert_template_pristine "$case_name"
}

assert_alias_refused alias-same-path-refused "$TEMPLATE"
pass alias-same-path-refused

ln -sf "$TEMPLATE" "$SANDBOX/link-to-template.hda"
assert_alias_refused alias-symlink-refused "$SANDBOX/link-to-template.hda"
pass alias-symlink-refused

ln -f "$TEMPLATE" "$SANDBOX/hardlink-to-template.hda" 2>/dev/null &&
  {
    assert_alias_refused alias-hardlink-refused "$SANDBOX/hardlink-to-template.hda"
    pass alias-hardlink-refused
  } ||
  printf '  [skip] the filesystem refused a hard link\n'

# --- a missing template refuses before the emulator starts -------------------
# Letting the launch proceed would have MAME create an empty disk and boot
# nothing, which reads as a broken image rather than a misconfigured path.
rm -f "$SANDBOX/mame.argv"
rm -rf "$SANDBOX/build/mame-run"
cat >"$SANDBOX/missing.env" <<EOF
MAME_EXECUTABLE=$SANDBOX/fake-mame
MAME_HDA=$SANDBOX/absent.hda
MAME_HOMEPATH=$SANDBOX/mame-home
EOF
if env -u WSL_INTEROP MAME_ENV_FILE="$SANDBOX/missing.env" \
    bash "$SANDBOX/scripts/mame-run.sh" \
    >"$SANDBOX/missing-template.log" 2>&1; then
  fail missing-template-refused "a missing template was accepted"
fi
grep -q "boot hard disk template not found" "$SANDBOX/missing-template.log" ||
  fail missing-template-refused "the refusal did not explain itself"
[ ! -f "$SANDBOX/mame.argv" ] ||
  fail missing-template-refused "the emulator was launched despite the refusal"
pass missing-template-refused

# --- the PowerShell twin, on a Windows-hosted rig -----------------------------
# On WSL the shell launcher re-execs into mame-run.ps1, so the twin is what
# actually runs on a Windows-hosted rig. The cases below drive a .cmd stub and
# translate paths with wslpath, so the requirement is Windows PowerShell
# specifically -- not any PowerShell. The Linux CI runners ship pwsh, which
# would enter this block and fail on the stub, so gate on powershell.exe alone.
# Where it is absent the block announces itself rather than passing silently.
POWERSHELL="$(command -v powershell.exe || true)"
if [ -z "$POWERSHELL" ]; then
  printf '  [skip] no Windows PowerShell here, so mame-run.ps1 was not exercised\n'
  exit 0
fi

cp "$SUBJECT_PS1" "$SANDBOX/scripts/mame-run.ps1"

to_host_path() {
  if command -v wslpath >/dev/null 2>&1; then
    wslpath -w "$1"
  else
    printf '%s' "$1"
  fi
}

cat >"$SANDBOX/fake-mame.cmd" <<'EOF'
@echo off
>"%~dp0mame.argv.txt" echo %*
exit /b 0
EOF

PS_TEMPLATE="$SANDBOX/ps-template.hda"
printf 'PRISTINE-TEMPLATE-BYTES' > "$PS_TEMPLATE"
PS_TEMPLATE_DIGEST="$(sha256sum "$PS_TEMPLATE" | cut -d' ' -f1)"
cat >"$SANDBOX/ps.env" <<EOF
MAME_MACHINE=maciix
MAME_EXECUTABLE=$(to_host_path "$SANDBOX/fake-mame.cmd")
MAME_HDA=$(to_host_path "$PS_TEMPLATE")
MAME_HOMEPATH=$(to_host_path "$SANDBOX/mame-home")
EOF

rm -rf "$SANDBOX/build/mame-run" "$SANDBOX/mame.argv.txt"
"$POWERSHELL" -NoProfile -ExecutionPolicy Bypass \
  -File "$(to_host_path "$SANDBOX/scripts/mame-run.ps1")" \
  -EnvironmentFile "$(to_host_path "$SANDBOX/ps.env")" \
  >"$SANDBOX/ps-boots-a-copy.log" 2>&1 ||
  fail ps-boots-a-copy "the PowerShell launcher exited non-zero"
tr -d '\r' < "$SANDBOX/mame.argv.txt" | grep -Fq 'mame-run' ||
  fail ps-boots-a-copy "the emulator did not receive a boot copy path"
tr -d '\r' < "$SANDBOX/mame.argv.txt" | grep -Fq "$(basename "$PS_TEMPLATE")" &&
  fail ps-boots-a-copy "the template itself was handed to the emulator"
[ "$(sha256sum "$PS_TEMPLATE" | cut -d' ' -f1)" = "$PS_TEMPLATE_DIGEST" ] ||
  fail ps-boots-a-copy "the template was modified"
pass ps-boots-a-copy

MAME_BOOT_HDA_HOST="$(to_host_path "$PS_TEMPLATE")"
cat >>"$SANDBOX/ps.env" <<EOF
MAME_BOOT_HDA=$MAME_BOOT_HDA_HOST
EOF
rm -f "$SANDBOX/mame.argv.txt"
if "$POWERSHELL" -NoProfile -ExecutionPolicy Bypass \
    -File "$(to_host_path "$SANDBOX/scripts/mame-run.ps1")" \
    -EnvironmentFile "$(to_host_path "$SANDBOX/ps.env")" \
    >"$SANDBOX/ps-alias-refused.log" 2>&1; then
  fail ps-alias-refused "an aliased boot copy was accepted"
fi
grep -q "resolves to the boot template itself" "$SANDBOX/ps-alias-refused.log" ||
  fail ps-alias-refused "the refusal did not explain itself"
[ ! -f "$SANDBOX/mame.argv.txt" ] ||
  fail ps-alias-refused "the emulator was launched despite the refusal"
[ "$(sha256sum "$PS_TEMPLATE" | cut -d' ' -f1)" = "$PS_TEMPLATE_DIGEST" ] ||
  fail ps-alias-refused "the template was modified"
pass ps-alias-refused
