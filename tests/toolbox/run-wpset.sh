#!/usr/bin/env bash
# Watchpoint (Classic ASan-analog) leg of the #230 scenario harness.
#
#   tests/toolbox/run-wpset.sh clean     - teardown touches no freed bag memory
#   tests/toolbox/run-wpset.sh control   - positive control: a watch on a live
#                                          bag buffer must fire on redraw
#
# Composition of proven pieces: the retro68-68k-dwarf build, the readelf
# relocation-free-function recipe (docs/MAME_DEVELOPMENT.md), and
# scripts/mame-debug.sh find/attach with LOKA_DEV_DATA + LOKA_GDB_SCRIPT.
# gdb reads members through `this` at symbol breakpoints (A5 globals are
# unreadable, arguments and heap objects are fine), so the freed buffer
# addresses come from `this->reader_.state_.bagBase[...]` with no test seam
# in the product.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

MODE="${1:-}"
if [ "$MODE" != "clean" ] && [ "$MODE" != "control" ]; then
  echo "Usage: $0 clean|control" >&2
  exit 2
fi

WORK="$PROJECT_DIR/build/mame-scenario/wpset-$MODE"
rm -rf "$WORK"
mkdir -p "$WORK"

fail_stage() {
  local stage="$1"
  shift
  echo "$stage stage failed: $*" >&2
  echo "Work directory left for inspection: $WORK" >&2
  exit 1
}

APPL="$PROJECT_DIR/build/retro68/68k/DiagDwarf4/tests/toolbox/LokaTestsToolbox68K.bin"
ELF="${APPL%.bin}.code.bin.gdb"
if [ ! -f "$ELF" ]; then
  fail_stage build \
    "missing $ELF; build it with: cmake --preset retro68-68k-dwarf && cmake --build --preset retro68-68k-dwarf --target LokaTestsToolbox68K_APPL"
fi

# --- pattern words: longest relocation-free function (docs recipe) ---------
PATTERN_FILE="$WORK/pattern"
python3 - "$ELF" >"$PATTERN_FILE" <<'PY'
import struct, subprocess, sys

elf = sys.argv[1]
section = ".code00002"
sections = subprocess.check_output(["readelf", "-SW", elf]).decode()
addr = offset = size = None
for line in sections.splitlines():
    if section in line:
        parts = line.split()
        i = parts.index(section)
        addr = int(parts[i + 2], 16)
        offset = int(parts[i + 3], 16)
        size = int(parts[i + 4], 16)
        break
if addr is None:
    raise SystemExit("section %s not found" % section)

relocs = []
in_section = False
for line in subprocess.check_output(["readelf", "-rW", elf]).decode().splitlines():
    if line.startswith("Relocation section"):
        in_section = ("rela" + section) in line
        continue
    if in_section:
        parts = line.split()
        if parts and all(c in "0123456789abcdef" for c in parts[0]) and len(parts[0]) == 8:
            relocs.append(int(parts[0], 16))
relocs.sort()

import bisect
best = None
for line in subprocess.check_output(["readelf", "-sW", elf]).decode().splitlines():
    parts = line.split()
    if len(parts) >= 8 and parts[3] == "FUNC":
        faddr = int(parts[1], 16)
        fsize = int(parts[2]) if parts[2].isdigit() else 0
        if fsize < 64 or faddr < addr or faddr >= addr + size:
            continue
        lo = bisect.bisect_left(relocs, faddr)
        if lo < len(relocs) and relocs[lo] < faddr + fsize:
            continue
        if best is None or fsize > best[1]:
            best = (faddr, fsize)
if best is None:
    raise SystemExit("no relocation-free function found")

data = open(elf, "rb").read()
foff = offset + (best[0] - addr)
w0, w1 = struct.unpack_from(">II", data, foff)
print("%08x %08x %08x" % (w0, w1, best[0]))
PY
read -r W0 W1 LINK <"$PATTERN_FILE"
echo "pattern: $W0 $W1 link $LINK"

# --- scenario config -------------------------------------------------------
CONFIG="$WORK/LokaTest.cfg"
printf 'scenario startup\nlinger_seconds 600\n' >"$CONFIG"
export LOKA_DEV_DATA="$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP
$CONFIG"

# --- load base: find phase, cached per ELF content -------------------------
ELF_SUM="$(md5sum "$ELF" | cut -d' ' -f1)"
BASE_CACHE="$PROJECT_DIR/build/mame-scenario/wpset-base-$ELF_SUM"
if [ -f "$BASE_CACHE" ]; then
  BASE="$(cat "$BASE_CACHE")"
  echo "base (cached): $BASE"
else
  rm -rf "$PROJECT_DIR/build/mame-debug"
  if ! timeout 420 "$PROJECT_DIR/scripts/mame-debug.sh" find "$APPL" "$W0" "$W1" "$LINK" >"$WORK/find.out" 2>&1; then
    fail_stage find "see $WORK/find.out"
  fi
  BASE="$(grep -o 'BASE 0x[0-9a-f]*' "$PROJECT_DIR/build/mame-debug/find-base.log" | head -1 | cut -d' ' -f2)"
  [ -n "$BASE" ] || fail_stage find "no BASE in find-base.log"
  printf '%s\n' "$BASE" >"$BASE_CACHE"
  echo "base (found): $BASE"
fi

# --- gdb batch script ------------------------------------------------------
GDB_SCRIPT="$WORK/wpset.gdb"
GDB_LOG="$WORK/wpset.log"
if [ "$MODE" = "clean" ]; then
  cat >"$GDB_SCRIPT" <<EOF
set logging file $GDB_LOG
set logging overwrite on
set logging enabled on
delete
# open() defensively closes first, so an unconditional break stops during
# attach with nothing loaded and the whole leg passes vacuously. Only the
# teardown close -- open package with a committed page -- may stop.
break scrapbook::ScrapbookPackage::close if this->open_ && this->currentBag_ >= 0
continue
echo LOKA-WPSET: close hit\n
set \$ui = (this->reader_.state_.bags[0].open ? (unsigned long)this->reader_.state_.bagBase[0] : 0)
set \$uisize = (this->reader_.state_.bags[0].open ? (unsigned long)this->reader_.state_.bags[0].storedSize : 0)
set \$cur = (this->currentBag_ >= 0 ? (unsigned long)this->reader_.state_.bagBase[this->currentBag_] : 0)
set \$cursize = (this->currentBag_ >= 0 ? (unsigned long)this->reader_.state_.bags[this->currentBag_].storedSize : 0)
set \$idx = (unsigned long)this->indexBytes_._M_impl._M_start
set \$idxsize = (unsigned long)(this->indexBytes_._M_impl._M_finish - this->indexBytes_._M_impl._M_start)
printf "LOKA-WPSET: ui=0x%lx+%lu cur=0x%lx+%lu idx=0x%lx+%lu\n", \$ui, \$uisize, \$cur, \$cursize, \$idx, \$idxsize
# Arm BEFORE close() runs: the releases happen inside close (releaseUiBag,
# the current blob reset, the reader/index teardown), so arming after a
# finish would leave the rest of close's own body unobserved. Verified on
# the rig that close touches no payload bytes itself, so live-at-entry
# watches do not false-positive.
echo LOKA-WPSET: arming full ranges before close runs\n
if \$ui != 0 && \$uisize != 0
  eval "awatch *(char(*)[%lu])0x%lx", \$uisize, \$ui
end
if \$cur != 0 && \$cursize != 0
  eval "awatch *(char(*)[%lu])0x%lx", \$cursize, \$cur
end
if \$idx != 0 && \$idxsize != 0
  eval "awatch *(char(*)[%lu])0x%lx", \$idxsize, \$idx
end
echo LOKA-WPSET: ARMED\n
info watchpoints
echo LOKA-WPSET: ARMED-END\n
# The definitive teardown-complete marker: reached only after the App and
# PlatformContext are destroyed (no fixed frame-count unwinding).
break loka::toolbox_tests::ScenarioTeardownComplete
continue
delete
echo LOKA-WPSET: CLEAN\n
quit
EOF
else
  cat >"$GDB_SCRIPT" <<EOF
set logging file $GDB_LOG
set logging overwrite on
set logging enabled on
delete
break scrapbook::ScrapbookPackage::commitPage
continue
echo LOKA-WPSET: commitPage hit\n
set \$live = (unsigned long)this->reader_.state_.bagBase[page.bag]
set \$livesize = (unsigned long)this->reader_.state_.bags[page.bag].storedSize
printf "LOKA-WPSET: live=0x%lx+%lu\n", \$live, \$livesize
finish
eval "rwatch *(char(*)[%lu])0x%lx", \$livesize, \$live
echo LOKA-WPSET: ARMED\n
info watchpoints
echo LOKA-WPSET: ARMED-END\n
continue
delete
echo LOKA-WPSET: CONTROL-DONE\n
quit
EOF
fi

export LOKA_GDB_SCRIPT="$GDB_SCRIPT"
timeout 480 "$PROJECT_DIR/scripts/mame-debug.sh" attach "$APPL" "$W0" "$W1" "$LINK" "$BASE" \
  >"$WORK/attach.out" 2>&1 || true

[ -f "$GDB_LOG" ] || fail_stage gdb "no gdb log; see $WORK/attach.out"

FIRED=0
if grep -qE "Old value|New value|^Value = " "$GDB_LOG"; then
  FIRED=1
fi
if grep -q "Could not insert" "$GDB_LOG"; then
  fail_stage verdict "the stub rejected a watchpoint; see $GDB_LOG"
fi

if [ "$MODE" = "clean" ]; then
  grep -q "LOKA-WPSET: ARMED" "$GDB_LOG" || fail_stage verdict "watches never armed; see $GDB_LOG"
  ARMED_COUNT="$(sed -n '/LOKA-WPSET: ARMED$/,/LOKA-WPSET: ARMED-END/p' "$GDB_LOG" | grep -c "watchpoint" || true)"
  if [ "${ARMED_COUNT:-0}" -lt 2 ]; then
    fail_stage verdict "only $ARMED_COUNT watchpoints armed (need >=2); the leg would be vacuous; see $GDB_LOG"
  fi
  if [ "$FIRED" -eq 1 ]; then
    fail_stage verdict "a watchpoint fired on freed memory during teardown; see $GDB_LOG"
  fi
  grep -q "ScenarioTeardownComplete" "$GDB_LOG" && grep -qE "Breakpoint [0-9]+, loka::toolbox_tests::ScenarioTeardownComplete" "$GDB_LOG" \
    || fail_stage verdict "the teardown-complete anchor was never reached; see $GDB_LOG"
  grep -q "LOKA-WPSET: CLEAN" "$GDB_LOG" || fail_stage verdict "teardown did not complete under watch; see $GDB_LOG"
  echo "wpset clean: teardown touched no freed bag memory"
else
  grep -q "LOKA-WPSET: ARMED" "$GDB_LOG" || fail_stage verdict "control watch never armed; see $GDB_LOG"
  if [ "$FIRED" -ne 1 ]; then
    fail_stage verdict "control watch on a live bag buffer never fired; see $GDB_LOG"
  fi
  echo "wpset control: watch on a live bag buffer fired as expected"
fi
