#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
  echo "Usage: $0 <example> <scenario from scenarios.txt> [--update-golden]" >&2
}

fail_stage() {
  local stage="$1"
  shift
  echo "$stage stage failed: $*" >&2
  echo "Work directory left for inspection: $WORK" >&2
  exit 1
}

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  usage
  exit 2
fi

EXAMPLE="$1"
SCENARIO="$2"
SCENARIO_REGISTRY="$SCRIPT_DIR/scenarios.txt"
UPDATE_GOLDEN=0
if [[ ! "$EXAMPLE" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || [[ ! "$SCENARIO" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || ! grep -Fxq -- "$EXAMPLE $SCENARIO" "$SCENARIO_REGISTRY"; then
  usage
  exit 2
fi
if [ $# -eq 3 ]; then
  if [ "$3" != "--update-golden" ]; then
    usage
    exit 2
  fi
  UPDATE_GOLDEN=1
fi

WORK="$PROJECT_DIR/build/mame-scenario/$EXAMPLE/$SCENARIO"
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

case "$EXAMPLE" in
  scrapbook)
    APPL="$PROJECT_DIR/build/retro68/68k/Release/tests/toolbox/LokaTestsToolbox68K.bin"
    TARGET="LokaTestsToolbox68K_APPL"
    FINDER_TAB_COUNT=1
    ;;
  helloworld)
    APPL="$PROJECT_DIR/build/retro68/68k/Release/tests/toolbox/LokaHelloWorldTestsToolbox68K.bin"
    TARGET="LokaHelloWorldTestsToolbox68K_APPL"
    FINDER_TAB_COUNT=2
    ;;
  *)
    fail_stage mame "unsupported example '$EXAMPLE'"
    ;;
esac
if [ ! -f "$APPL" ]; then
  fail_stage mame \
    "missing $APPL; build it with: cmake --preset retro68-68k-release && cmake --build --preset retro68-68k-release --target $TARGET"
fi
LRPC="$PROJECT_DIR/build/host/lrpc/lrpc"
if [ "$EXAMPLE" = "scrapbook" ] && [ ! -x "$LRPC" ]; then
  fail_stage mame \
    "missing $LRPC; build it with: cmake -S tools/lrpc -B build/host/lrpc && cmake --build build/host/lrpc"
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
LAUNCH_LOG="$WORK/mame-launch.log"
AUDIT="$WORK/LokaTestsToolbox.audit"
EXPECTED_AUDIT="$PROJECT_DIR/tests/scenarios/expected/$EXAMPLE/$SCENARIO.audit"
ACTUAL_IMAGE="$WORK/$SCENARIO.png"
# Goldens are rig-local, not tracked: the pixels depend on the local boot
# image's System resources (fonts, control chrome) and contain Apple-rendered
# glyphs, which the licensing rule keeps out of the tree. They survive work
# directory wipes and regenerate with --update-golden; reviewers see the
# captures through the pr-assets evidence branch instead.
GOLDEN="$PROJECT_DIR/build/mame-scenario/golden/$EXAMPLE/$SCENARIO.png"
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
# linger_seconds keeps the scenario window alive after the audit is written,
# so the emulator-side snapshot captures the scene instead of the desktop the
# application would otherwise have quit back to.
if ! printf 'scenario %s\nlinger_seconds 120\n' "$SCENARIO" >"$CONFIG"; then
  fail_stage mame "could not write LokaTest.cfg"
fi
if ! cp -f "$SCRIPT_DIR/mame-launch.lua" "$LAUNCHER"; then
  fail_stage mame "could not stage mame-launch.lua"
fi

DEV_DISK_ARGUMENTS=("$APPL")
if [ "$EXAMPLE" = "scrapbook" ]; then
  ASSETS="$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP"
  if [ ! -f "$ASSETS" ]; then
    fail_stage mame "package not found: $ASSETS"
  fi
  STAGED_ASSETS="$WORK/ASSETS.LRP"
  CORRUPT_BAG=""
  case "$SCENARIO" in
    open-first-page-refused) CORRUPT_BAG=1 ;;
    refused-flip-keeps-page) CORRUPT_BAG=3 ;;
    open-text-page-refused) CORRUPT_BAG=5 ;;
  esac
  STAGE_ARGUMENTS=(stage "$ASSETS" -o "$STAGED_ASSETS")
  if [ -n "$CORRUPT_BAG" ]; then
    STAGE_ARGUMENTS+=(--corrupt-bag "$CORRUPT_BAG")
  fi
  if ! "$LRPC" "${STAGE_ARGUMENTS[@]}"; then
    fail_stage mame "could not stage the scenario package"
  fi
  DEV_DISK_ARGUMENTS+=("$STAGED_ASSETS")
fi
DEV_DISK_ARGUMENTS+=("$CONFIG")

# A scenario-local control dir keeps mame-dev-disk.sh's hfsutils state
# (current mounted volume) isolated, so parallel ctest runs of the two
# scenarios cannot cross-mount each other's development disks.
if ! MAME_DEV_HDA="$DEV" MAME_CONTROL_DIR="$WORK/hfs-ctl" \
    "$PROJECT_DIR/scripts/mame-dev-disk.sh" \
    "${DEV_DISK_ARGUMENTS[@]}" >"$WORK/dev-disk.out" 2>&1; then
  fail_stage mame "development disk creation failed; see $WORK/dev-disk.out"
fi

if [ -z "${LOKA_TAB_COUNT:-}" ]; then
  LOKA_TAB_COUNT="$FINDER_TAB_COUNT"
fi
export LOKA_TAB_COUNT
export LOKA_SNAP_LOG; LOKA_SNAP_LOG="$(winpath "$LAUNCH_LOG")"
FORWARD="LOKA_SNAP_LOG:LOKA_TAB_COUNT"
if [ -n "${LOKA_LAUNCH_WAIT:-}" ]; then
  FORWARD="$FORWARD:LOKA_LAUNCH_WAIT"
fi
if [ -n "${LOKA_SETTLE_TIMEOUT:-}" ]; then
  FORWARD="$FORWARD:LOKA_SETTLE_TIMEOUT"
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
SETTLE_REACHED=1
if [ ! -f "$LAUNCH_LOG" ] \
  || ! grep -q '^LOKA-SNAP: settled after ' "$LAUNCH_LOG"; then
  SETTLE_REACHED=0
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
if ! HOME="$HFS_HOME" "$HCOPY" -t ":LokaTestsToolbox.audit" "$AUDIT" >"$WORK/hcopy.out" 2>&1; then
  HOME="$HFS_HOME" "$HUMOUNT" >/dev/null 2>&1 || true
  fail_stage extract "could not copy LokaTestsToolbox.audit; see $WORK/hcopy.out"
fi
if ! HOME="$HFS_HOME" "$HUMOUNT" >"$WORK/humount.out" 2>&1; then
  fail_stage extract "could not unmount the development disk; see $WORK/humount.out"
fi

if [ ! -f "$EXPECTED_AUDIT" ]; then
  fail_stage verdict "missing tracked audit $EXPECTED_AUDIT"
fi
if ! cmp "$EXPECTED_AUDIT" "$AUDIT"; then
  fail_stage verdict "audit differs from $EXPECTED_AUDIT; see $AUDIT"
fi
if [ "$SETTLE_REACHED" -ne 1 ]; then
  fail_stage settle "pixel stability was not reached; see $LAUNCH_LOG"
fi

newest_snapshot=""
while IFS= read -r -d '' candidate; do
  if [ -z "$newest_snapshot" ] || [ "$candidate" -nt "$newest_snapshot" ]; then
    newest_snapshot="$candidate"
  fi
done < <(find "$SNAPSHOT_DIR" -type f -name '*.png' -print0)
if [ -z "$newest_snapshot" ]; then
  fail_stage crop "MAME did not write a snapshot PNG under $SNAPSHOT_DIR"
fi
PNG_TOOL="$PROJECT_DIR/tests/scenarios/pngtool.py"
if ! cp -f "$newest_snapshot" "$ACTUAL_IMAGE"; then
  fail_stage collect "could not collect $newest_snapshot"
fi

if [ "$UPDATE_GOLDEN" -eq 1 ]; then
  if ! mkdir -p "$(dirname "$GOLDEN")"; then
    fail_stage golden "could not create the golden directory"
  fi
  if ! cp -f "$ACTUAL_IMAGE" "$GOLDEN"; then
    fail_stage golden "could not update $GOLDEN"
  fi
  echo "Updated golden: $GOLDEN"
  echo "Reminder: attach before/after visual evidence to the PR."
  exit 0
fi

if [ ! -f "$GOLDEN" ]; then
  fail_stage golden "missing $GOLDEN; rerun with --update-golden to create it"
fi
if ! python3 "$PNG_TOOL" compare "$ACTUAL_IMAGE" "$GOLDEN"; then
  python3 "$PNG_TOOL" diff "$GOLDEN" "$ACTUAL_IMAGE" "$DIFF_DIR/$SCENARIO.png" || true
  fail_stage golden "settled snapshot differs from $GOLDEN"
fi

echo "Scenario passed: $EXAMPLE/$SCENARIO"
