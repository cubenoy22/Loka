#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ $# -gt 1 ] || { [ $# -eq 1 ] && [ "$1" != --stage-last ] && [ "$1" != --update-golden ]; }; then
  echo "Usage: $0 [--stage-last | --update-golden]" >&2
  exit 2
fi

# Keep child logs outside WORK: normal runs wipe their own work directory.
LOG_DIR="$PROJECT_DIR/build/mame-scenario/batch"
mkdir -p "$LOG_DIR"
ROWS=()
BLOCKED=()
RESULT=0
while read -r example scenario; do
  [ -n "$example" ] || continue
  cell="$example/$scenario"
  work="$PROJECT_DIR/build/mame-scenario/$cell"
  log="$LOG_DIR/$example-$scenario.log"
  status=0
  "$SCRIPT_DIR/run-scenario.sh" "$example" "$scenario" "$@" >"$log" 2>&1 </dev/null || status=$?
  cat "$log"
  outcome="$(python3 - "$status" "$log" <<'PY'
import pathlib
import re
import sys

status = int(sys.argv[1])
log = pathlib.Path(sys.argv[2]).read_text(errors="replace")
stages = re.findall(r"^([\w-]+) stage failed: (.*)$", log, re.MULTILINE)
if status == 0:
    print("pass")
elif re.search(r"^machine_verdict=refused$", log, re.MULTILINE):
    print("refused")
elif stages:
    stage, message = stages[-1]
    if stage == "verdict" and message.startswith("audit differs from "):
        print("audit-differs")
    elif stage == "provenance":
        print("refused")
    elif stage == "golden" and message.startswith("settled snapshot differs from "):
        pixels = re.findall(r"^compare result: .*?differing pixels: (\d+);", log, re.MULTILINE)
        print("differs " + pixels[-1] if pixels else "failed golden")
    else:
        print("failed " + stage)
else:
    print("failed unknown")
PY
)"
  ROWS+=("$cell | $outcome | $work")
  if [ "$outcome" != pass ]; then RESULT=1; fi
  case "$outcome" in
    pass|differs\ *) ;;
    *) BLOCKED+=("$cell") ;;
  esac
done <"$PROJECT_DIR/tests/scenarios/scenarios.txt"

printf '\ncell | outcome | work dir\n'
printf '%s\n' "${ROWS[@]}"
if [ "${#BLOCKED[@]}" -eq 0 ]; then
  echo "bake: possible"
else
  printf 'bake: blocked by %s cell(s):' "${#BLOCKED[@]}"
  printf ' %s' "${BLOCKED[@]}"
  printf '\n'
fi
exit "$RESULT"
