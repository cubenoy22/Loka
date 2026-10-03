#!/usr/bin/env bash
# Partition sizing rail for the Classic examples (#1103).
#
#   tests/toolbox/measure-example-heaps.sh [example ...]
#
# For each example, boot MAME, launch the production 68K build, play a fixed
# workload, and report the partition it needs:
#
#   need = peak live bytes of the application zone (mame-measure-heap.lua)
#        + the partition's fixed part (stack, A5 world, zone header), read
#          as the declared preferred size minus the zone's maximum size
#
# and the values the Size.r files are set from: minimum = need x 1.1 rounded
# up to 32K, preferred = need x 1.5 rounded up to 64K. The formula was checked
# against real boundaries on 2026-10-03: HelloWorld (need 392K) dies at launch
# in 384K and runs in 416K; LazyList (434K) dies in 416K and runs in 448K;
# MineSweeper (440K) aborts in 432K and runs in 440K. The values are for 68K
# only; Size.r keeps the unmeasured PPC partitions in their own branch.
#
# The rail does not build. Build first:
#   cmake --build --preset retro68-68k-release
# One example takes about three minutes; separate invocations may run in
# parallel because each example has its own work directory.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
ALL_EXAMPLES=(tutorial floppybird smirkbench scrapbook helloworld lazylist minesweeper simpleviewer)
fail() { echo "measure-example-heaps: $*" >&2; exit 1; }

. "$PROJECT_DIR/scripts/retro68-env.sh"
loka_load_retro68_environment "$PROJECT_DIR"
ENV_FILE="${MAME_ENV_FILE:-$PROJECT_DIR/.env-mame}"
[ -f "$ENV_FILE" ] && loka_import_environment_file "$ENV_FILE"
normalize_host_path() {
  if [[ "$1" =~ ^[A-Za-z]:\\ ]] && command -v wslpath >/dev/null 2>&1; then wslpath -u "$1"; else printf '%s' "$1"; fi
}
winpath() {
  if [ -n "${WSL_INTEROP:-}" ]; then wslpath -w "$1"; else printf '%s' "$1"; fi
}
retro68_tool() {
  local candidate
  for candidate in "${RETRO68_TOOLCHAIN_BIN:-}/$1" "${RETRO68_BUILD_DIR:-$HOME/Retro68-build}/toolchain/bin/$1"; do
    [ -x "$candidate" ] && { printf '%s\n' "$candidate"; return; }
  done
  command -v "$1" || fail "Retro68 tool not found: $1"
}
MAME_EXECUTABLE="$(normalize_host_path "${MAME_EXECUTABLE:-mame}")"
MAME_HDA="$(normalize_host_path "${MAME_HDA:-}")"
[ -f "$MAME_HDA" ] || fail 'MAME_HDA must point to the boot template'
RELEASE="$PROJECT_DIR/build/retro68/68k/Release/example"

# Workloads use the production window positions on maciix (640x480). Tab
# counts are the Finder's landing for that disk's item set.
minesweeper_steps() {
  local steps="" round i cell
  for round in 1 2; do
    for i in $(seq 0 63); do
      cell=$(( (i * 37 + round * 11) % 64 ))
      steps+="c $((44 + 24 * (cell % 8))) $((99 + 21 * (cell / 8)));"
    done
    steps+="s;c 130 73;w 3;"
  done
  printf '%s' "${steps}s;"
}
lazylist_steps() {
  local steps="c 412 111;c 412 111;c 412 111;c 566 111;c 566 111;c 97 111;c 254 111;w 2;s;" i
  for i in $(seq 1 12); do steps+="c 634 330;w 1;"; done
  steps+="s;"
  for i in $(seq 1 12); do steps+="c 634 170;w 1;"; done
  printf '%s' "${steps}s;"
}

measure() {
  local example="$1" dir app sizer tabs steps extras=()
  case "$example" in
    tutorial) dir=Tutorial app=LokaTutorial68K; tabs=1; steps="w 10;s;" ;;
    floppybird) dir=FloppyBird app=LokaFloppyBird68K; tabs=1
      steps="K space;w 1;$(printf 'k space;w 1;%.0s' 1 2 3 4 5 6 7)w 5;s;w 10;s;" ;;
    smirkbench) dir=SmirkBench app=LokaSmirkBench68K; tabs=1
      steps="$(printf 'c 113 103;%.0s' 1 2 3 4 5)w 10;s;" ;;
    scrapbook) dir=ScrapbookUI app=ScrapbookUI68K; tabs=2
      extras=("$PROJECT_DIR/example/ScrapbookUI/ASSETS.LRP")
      steps="$(printf 'c 290 287;w 2;%.0s' 1 2 3 4)s;$(printf 'c 129 287;w 2;%.0s' 1 2 3 4)s;" ;;
    helloworld) dir=HelloWorld app=LokaHello68K; tabs=1
      steps="K space;w 1;c 157 157;c 157 157;c 157 157;c 157 205;c 157 229;c 157 229;c 345 133;w 2;c 345 280;k five;k zero;w 2;s;c 449 368;c 449 368;c 449 368;w 2;s;" ;;
    lazylist) dir=LazyList app=LokaLazyList68K; tabs=1; steps="$(lazylist_steps)" ;;
    minesweeper) dir=MineSweeper app=LokaMine68K; tabs=1; steps="$(minesweeper_steps)" ;;
    simpleviewer) dir=SimpleViewer app=LokaSimpleViewer68K; tabs=3
      # The open dialog lists Desktop DB and Desktop DF first: four Downs reach
      # Sun.pict and five reach Zbulb.pict (Bulb.pict renamed to sort last).
      steps="c 128 71;w 4;$(printf 'k down;%.0s' 1 2 3 4)k ret;w 8;s;c 128 143;w 2;c 128 167;w 2;c 128 191;w 2;s;"
      steps+="c 128 71;w 4;$(printf 'k down;%.0s' 1 2 3 4 5)k ret;w 8;s;c 128 143;w 2;c 128 191;w 2;s;" ;;
    *) fail "unknown example '$example' (known: ${ALL_EXAMPLES[*]})" ;;
  esac
  sizer="$PROJECT_DIR/example/$dir/Size.r"
  local bin="$RELEASE/$dir/$app.bin"
  [ -f "$bin" ] || fail "missing $bin; build retro68-68k-release first"
  # The 68K preferred size is the first value in the LOKA_CLASSIC_68K branch.
  local preferred_k
  preferred_k="$(awk '/^#elif LOKA_CLASSIC_68K/{block=1; next} block && /^#/{exit} block && /\* *1024/{gsub(/[^0-9*]/,""); split($0,p,"*"); print p[1]; exit}' "$sizer")"
  [[ "$preferred_k" =~ ^[0-9]+$ ]] || fail "could not read the preferred size from $sizer"

  local work="$PROJECT_DIR/build/mame-measure/$example"
  rm -rf "$work"
  mkdir -p "$work"/{home,cfg,nvram,snapshot,diff,hfs-ctl,hfs-home}
  if [ "$example" = simpleviewer ]; then
    local hmount hcopy humount
    hmount="$(retro68_tool hmount)"; hcopy="$(retro68_tool hcopy)"; humount="$(retro68_tool humount)"
    HOME="$work/hfs-home" "$hmount" "$MAME_HDA" >"$work/picture-hmount.out" 2>&1 || fail "could not mount the boot template"
    HOME="$work/hfs-home" "$hcopy" -r ":Desktop Folder:Images:Sun.pict" "$work/Sun.pict" >/dev/null 2>&1 \
      && HOME="$work/hfs-home" "$hcopy" -r ":Desktop Folder:Images:Bulb.pict" "$work/Zbulb.pict" >/dev/null 2>&1 \
      || { HOME="$work/hfs-home" "$humount" >/dev/null 2>&1 || true; fail "boot template lacks :Desktop Folder:Images:Sun.pict / Bulb.pict"; }
    HOME="$work/hfs-home" "$humount" >/dev/null 2>&1 || true
    extras=("$work/Sun.pict" "$work/Zbulb.pict")
  fi
  . "$PROJECT_DIR/scripts/mame-boot-copy.sh"
  loka_prepare_boot_copy "$MAME_HDA" "$work/Boot.hd" >/dev/null
  MAME_DEV_HDA="$work/LokaDev.hd" MAME_CONTROL_DIR="$work/hfs-ctl" \
    "$PROJECT_DIR/scripts/mame-dev-disk.sh" "$bin" "${extras[@]}" >"$work/dev-disk.out" 2>&1 \
    || fail "dev disk creation; see $work/dev-disk.out"
  cp "$SCRIPT_DIR/mame-measure-heap.lua" "$work/mame-measure-heap.lua"
  local args=(
    "${MAME_MACHINE:-maciix}" -ramsize "${MAME_RAMSIZE:-8M}"
    -homepath "$(winpath "$work/home")" -cfg_directory "$(winpath "$work/cfg")"
    -nvram_directory "$(winpath "$work/nvram")" -snapshot_directory "$(winpath "$work/snapshot")"
    -diff_directory "$(winpath "$work/diff")" -hard1 "$(winpath "$work/Boot.hd")"
    -scsi:5 harddisk -hard2 "$(winpath "$work/LokaDev.hd")"
    -video none -sound none -nothrottle -natural -skip_gameinfo
    -autoboot_delay 1 -autoboot_script "$(winpath "$work/mame-measure-heap.lua")"
  )
  [ -n "${MAME_ROMPATH:-}" ] && args+=(-rompath "$MAME_ROMPATH")
  LOKA_SNAP_LOG="$(winpath "$work/measure.log")" LOKA_TAB_COUNT="$tabs" LOKA_STEPS="$steps" \
    WSLENV="${WSLENV:+$WSLENV:}LOKA_SNAP_LOG:LOKA_TAB_COUNT:LOKA_STEPS" \
    timeout 1200 "$MAME_EXECUTABLE" "${args[@]}" >"$work/mame.out" 2>&1 </dev/null \
    || fail "$example: MAME exited with status $?; see $work/mame.out"
  local log
  log="$(tr -d '\r' < "$work/measure.log")"
  grep -qx 'LOKA-MEASURE: complete' <<<"$log" || fail "$example: probe did not complete; see $work/measure.log"
  local live max
  live="$(sed -n 's/^PEAK live=\([0-9]*\) .*/\1/p' <<<"$log")"
  max="$(sed -n 's/^launched max=\([0-9]*\)$/\1/p' <<<"$log")"
  [ -n "$live" ] && [ -n "$max" ] || fail "$example: the application did not survive the workload; see $work/measure.log and $work/snapshot"
  awk -v ex="$example" -v live="$live" -v max="$max" -v pref="$preferred_k" '
  # Round a fractional K value up to a multiple of step (int() truncates).
  function ceil_to(value, step,    n) { n = int(value / step); if (n * step < value) n++; return n * step }
  BEGIN {
    fixed = pref * 1024 - max; need = live + fixed
    min = ceil_to(need * 1.1 / 1024, 32); best = ceil_to(need * 1.5 / 1024, 64)
    printf "%-13s need=%5.1fK (live %d + fixed %d)  minimum=%dK  preferred=%dK  (declared preferred %dK)\n",
      ex, need / 1024, live, fixed, min, best, pref
  }'
}

[ "$#" -gt 0 ] || set -- "${ALL_EXAMPLES[@]}"
for example in "$@"; do
  measure "$example"
done
