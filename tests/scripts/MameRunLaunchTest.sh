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
