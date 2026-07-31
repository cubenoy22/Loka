#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
  echo "Usage: $0 <open-first-page|open-first-page-refused> [--update-golden]" >&2
}

fail_stage() {
  local stage="$1"
  shift
  echo "$stage stage failed: $*" >&2
  echo "Work directory left for inspection: $WORK" >&2
  exit 1
}

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  usage
  exit 2
fi

SCENARIO="$1"
UPDATE_GOLDEN=0
case "$SCENARIO" in
  open-first-page|open-first-page-refused)
    ;;
  *)
    usage
    exit 2
    ;;
esac
if [ $# -eq 2 ]; then
  if [ "$2" != "--update-golden" ]; then
    usage
    exit 2
  fi
  UPDATE_GOLDEN=1
fi

WORK="$PROJECT_DIR/build/mame-scenario/$SCENARIO"
if [ -d "$WORK" ]; then
  if ! rm -rf "$WORK"; then
    fail_stage mame "could not wipe the previous work directory"
  fi
fi
if ! mkdir -p "$WORK"; then
  fail_stage mame "could not create the work directory"
fi

ENV_FILE="${MAME_ENV_FILE:-$PROJECT_DIR/.env-mame}"
if [ -f "$ENV_FILE" ]; then
  set -a
  # shellcheck source=/dev/null
  . "$ENV_FILE"
  set +a
fi

APPL="$PROJECT_DIR/build/retro68/68k/Release/tests/toolbox/LokaTestsToolbox68K.bin"
if [ ! -f "$APPL" ]; then
  fail_stage mame \
    "missing $APPL; build it with: cmake --preset retro68-68k-release && cmake --build --preset retro68-68k-release --target LokaTestsToolbox68K_APPL"
fi
if [ -z "${MAME_EXECUTABLE:-}" ]; then
  fail_stage mame "set MAME_EXECUTABLE in .env-mame"
fi
if [ -z "${MAME_HDA:-}" ]; then
  fail_stage mame "set MAME_HDA in .env-mame"
fi

MACHINE="${MAME_MACHINE:-maciix}"
RAMSIZE="${MAME_RAMSIZE:-8M}"
IS_WSL=0
if [ -n "${WSL_INTEROP:-}" ]; then IS_WSL=1; fi
winpath() { if [ "$IS_WSL" = "1" ]; then wslpath -w "$1"; else printf '%s' "$1"; fi; }

# .env-mame holds Windows paths on a Windows host; the shell needs the other
# form to copy files and to exec the emulator. Same idiom as mame-debug.sh.
normalize_host_path() {
  local path="$1"
  if [[ "$path" =~ ^[A-Za-z]:\\ ]] && command -v wslpath >/dev/null 2>&1; then
    wslpath -u "$path"
  else
    printf '%s' "$path"
  fi
}
MAME_EXECUTABLE="$(normalize_host_path "$MAME_EXECUTABLE")"
MAME_HDA="$(normalize_host_path "$MAME_HDA")"

if [ ! -f "$MAME_HDA" ]; then
  fail_stage mame "boot hard disk template not found: $MAME_HDA"
fi

BOOT="$WORK/Boot.hd"
DEV="$WORK/LokaDev.hd"
CONFIG="$WORK/LokaTest.cfg"
LAUNCHER="$WORK/mame-launch.lua"
MAME_OUT="$WORK/mame.out"
RECORD="$WORK/LokaTestsToolbox.snap"
CROPPED="$WORK/$SCENARIO.png"
GOLDEN="$PROJECT_DIR/tests/golden/scrapbook/$SCENARIO.png"
HOME_DIR="$WORK/home"
CFG_DIR="$WORK/cfg"
NVRAM_DIR="$WORK/nvram"
SNAPSHOT_DIR="$WORK/snapshot"
DIFF_DIR="$WORK/diff"
HFS_HOME="$WORK/hfs-home"
if ! mkdir -p "$HOME_DIR" "$CFG_DIR" "$NVRAM_DIR" "$SNAPSHOT_DIR" "$DIFF_DIR" "$HFS_HOME"; then
  fail_stage mame "could not create scenario-local runtime directories"
fi

if ! cp -f "$MAME_HDA" "$BOOT"; then
  fail_stage mame "could not copy the boot hard disk template"
fi
# linger_seconds keeps the scenario window alive after the record is written,
# so the emulator-side snapshot captures the scene instead of the desktop the
# application would otherwise have quit back to.
if ! printf 'scenario %s\nlinger_seconds 120\n' "$SCENARIO" >"$CONFIG"; then
  fail_stage mame "could not write LokaTest.cfg"
fi
if ! cp -f "$SCRIPT_DIR/mame-launch.lua" "$LAUNCHER"; then
  fail_stage mame "could not stage mame-launch.lua"
fi

ASSETS="$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP"
if [ ! -f "$ASSETS" ]; then
  fail_stage mame "package not found: $ASSETS"
fi
STAGED_ASSETS="$ASSETS"
if [ "$SCENARIO" = "open-first-page-refused" ]; then
  STAGED_ASSETS="$WORK/ASSETS.LRP"
  if ! cp -f "$ASSETS" "$STAGED_ASSETS"; then
    fail_stage mame "could not stage the package for corruption"
  fi
  # Offset 900 is inside bag 1's payload in the committed 3388-byte package.
  # Revisit this corruption point if the package layout or size changes.
  if [ "$(wc -c <"$STAGED_ASSETS")" -ne 3388 ]; then
    fail_stage mame "ASSETS.LRP is no longer 3388 bytes; verify the bag-1 corruption offset"
  fi
  if ! python3 - "$STAGED_ASSETS" <<'PY'
import sys

path = sys.argv[1]
with open(path, "r+b") as package:
    package.seek(900)
    original = package.read(1)
    if len(original) != 1:
        raise SystemExit("package is too short for offset 900")
    package.seek(900)
    package.write(bytes((original[0] ^ 0x01,)))
PY
  then
    fail_stage mame "could not corrupt bag 1 in the staged package"
  fi
fi

if ! MAME_DEV_HDA="$DEV" "$PROJECT_DIR/scripts/mame-dev-disk.sh" \
    "$APPL" "$STAGED_ASSETS" "$CONFIG" >"$WORK/dev-disk.out" 2>&1; then
  fail_stage mame "development disk creation failed; see $WORK/dev-disk.out"
fi

export LOKA_SNAP_LOG; LOKA_SNAP_LOG="$(winpath "$WORK/mame-launch.log")"
FORWARD="LOKA_SNAP_LOG"
if [ -n "${LOKA_LAUNCH_WAIT:-}" ]; then
  FORWARD="$FORWARD:LOKA_LAUNCH_WAIT"
fi
if [ -n "${LOKA_SETTLE_WAIT:-}" ]; then
  FORWARD="$FORWARD:LOKA_SETTLE_WAIT"
fi
# WSL hands nothing to a Windows process unless it is named here.
export WSLENV="${WSLENV:+$WSLENV:}$FORWARD"

ARGS=(
  "$MACHINE" -ramsize "$RAMSIZE"
  -homepath "$(winpath "$HOME_DIR")"
  -cfg_directory "$(winpath "$CFG_DIR")"
  -nvram_directory "$(winpath "$NVRAM_DIR")"
  -snapshot_directory "$(winpath "$SNAPSHOT_DIR")"
  -diff_directory "$(winpath "$DIFF_DIR")"
  -hard1 "$(winpath "$BOOT")"
  -scsi:5 harddisk -hard2 "$(winpath "$DEV")"
  -video none -sound none -nothrottle -natural -skip_gameinfo
  -autoboot_delay 1 -autoboot_script "$(winpath "$LAUNCHER")"
)
if [ -n "${MAME_ROMPATH:-}" ]; then
  ARGS+=(-rompath "$MAME_ROMPATH")
fi

if timeout 480 "$MAME_EXECUTABLE" "${ARGS[@]}" >"$MAME_OUT" 2>&1; then
  :
else
  mame_status=$?
  if [ "$mame_status" -eq 124 ]; then
    fail_stage mame "timed out after 480 seconds; see $MAME_OUT"
  fi
  fail_stage mame "MAME exited with status $mame_status; see $MAME_OUT"
fi

find_retro68_tool() {
  local name="$1"
  local candidate

  if [ -n "${RETRO68_TOOLCHAIN_BIN:-}" ] \
    && [ -x "$RETRO68_TOOLCHAIN_BIN/$name" ]; then
    echo "$RETRO68_TOOLCHAIN_BIN/$name"
    return
  fi
  if command -v "$name" >/dev/null 2>&1; then
    command -v "$name"
    return
  fi
  if [ -n "${RETRO68_BUILD_DIR:-}" ] \
    && [ -x "$RETRO68_BUILD_DIR/toolchain/bin/$name" ]; then
    echo "$RETRO68_BUILD_DIR/toolchain/bin/$name"
    return
  fi
  for candidate in \
    "$HOME/Retro68-build/toolchain/bin/$name" \
    "$HOME/Documents/Projects/Retro68-build/toolchain/bin/$name"; do
    if [ -x "$candidate" ]; then
      echo "$candidate"
      return
    fi
  done
  echo "Retro68 tool not found: $name" >&2
  return 1
}

if ! HMOUNT="$(find_retro68_tool hmount)"; then
  fail_stage extract "hmount is unavailable"
fi
if ! HCOPY="$(find_retro68_tool hcopy)"; then
  fail_stage extract "hcopy is unavailable"
fi
if ! HUMOUNT="$(find_retro68_tool humount)"; then
  fail_stage extract "humount is unavailable"
fi
if ! HOME="$HFS_HOME" "$HMOUNT" "$DEV" 1 >"$WORK/hmount.out" 2>&1; then
  fail_stage extract "could not mount the development disk; see $WORK/hmount.out"
fi
if ! HOME="$HFS_HOME" "$HCOPY" -t ":LokaTestsToolbox.snap" "$RECORD" >"$WORK/hcopy.out" 2>&1; then
  HOME="$HFS_HOME" "$HUMOUNT" >/dev/null 2>&1 || true
  fail_stage extract "could not copy LokaTestsToolbox.snap; see $WORK/hcopy.out"
fi
if ! HOME="$HFS_HOME" "$HUMOUNT" >"$WORK/humount.out" 2>&1; then
  fail_stage extract "could not unmount the development disk; see $WORK/humount.out"
fi

status=""
crop_left=""
crop_top=""
crop_right=""
crop_bottom=""
status_count=0
while IFS=$'\t' read -r key value; do
  case "$key" in
    status)
      status="$value"
      status_count=$((status_count + 1))
      ;;
    crop_left) crop_left="$value" ;;
    crop_top) crop_top="$value" ;;
    crop_right) crop_right="$value" ;;
    crop_bottom) crop_bottom="$value" ;;
  esac
done <"$RECORD"

if [ "$status_count" -ne 1 ]; then
  fail_stage verdict "record must contain exactly one status line; see $RECORD"
fi
if [ "$status" != "ok" ]; then
  fail_stage verdict "guest reported status '$status'; see $RECORD"
fi
for coordinate in "$crop_left" "$crop_top" "$crop_right" "$crop_bottom"; do
  if [[ ! "$coordinate" =~ ^[0-9]+$ ]]; then
    fail_stage verdict "record contains an invalid crop rectangle; see $RECORD"
  fi
done

newest_snapshot=""
while IFS= read -r -d '' candidate; do
  if [ -z "$newest_snapshot" ] || [ "$candidate" -nt "$newest_snapshot" ]; then
    newest_snapshot="$candidate"
  fi
done < <(find "$SNAPSHOT_DIR" -type f -name '*.png' -print0)
if [ -z "$newest_snapshot" ]; then
  fail_stage crop "MAME did not write a snapshot PNG under $SNAPSHOT_DIR"
fi
if ! python3 "$SCRIPT_DIR/pngcrop.py" crop "$newest_snapshot" \
    "$crop_left" "$crop_top" "$crop_right" "$crop_bottom" "$CROPPED"; then
  fail_stage crop "could not crop $newest_snapshot"
fi

if [ "$UPDATE_GOLDEN" -eq 1 ]; then
  if ! mkdir -p "$(dirname "$GOLDEN")"; then
    fail_stage golden "could not create the golden directory"
  fi
  if ! cp -f "$CROPPED" "$GOLDEN"; then
    fail_stage golden "could not update $GOLDEN"
  fi
  echo "Updated golden: $GOLDEN"
  echo "Reminder: attach before/after visual evidence to the PR."
  exit 0
fi

if [ ! -f "$GOLDEN" ]; then
  fail_stage golden "missing $GOLDEN; rerun with --update-golden to create it"
fi
if ! python3 "$SCRIPT_DIR/pngcrop.py" compare "$CROPPED" "$GOLDEN"; then
  fail_stage golden "cropped snapshot differs from $GOLDEN"
fi

echo "Scenario passed: $SCENARIO"
