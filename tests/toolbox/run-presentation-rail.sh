#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SCENARIO_REGISTRY="$SCRIPT_DIR/scrapbook-scenarios.txt"
SCENARIO_RUNNER="$SCRIPT_DIR/run-scenario.sh"
PRESENTATION_ROOT="$PROJECT_DIR/build/mame-scenario/presentation"

usage() {
  echo "Usage: $0" >&2
}

fail_stage() {
  echo "presentation stage failed: $*" >&2
  echo "Incomplete presentation directory left for inspection: $INCOMPLETE" >&2
  exit 1
}

if [ $# -ne 0 ]; then
  usage
  exit 2
fi

RUN_ID="${LOKA_TOOLBOX_PRESENTATION_RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)-$$}"
if [[ ! "$RUN_ID" =~ ^[A-Za-z0-9][A-Za-z0-9._-]*$ ]]; then
  echo "presentation stage failed: invalid run ID '$RUN_ID'" >&2
  exit 2
fi

if ! command -v flock >/dev/null 2>&1; then
  echo "presentation stage failed: flock is required to serialize presentation rails" >&2
  exit 1
fi
if ! mkdir -p "$PRESENTATION_ROOT"; then
  echo "presentation stage failed: could not create $PRESENTATION_ROOT" >&2
  exit 1
fi
# Every scenario uses build/mame-scenario/<scenario> as its live work
# directory. Keep one owner across the full rail so another run cannot wipe or
# collect those directories between scenario execution and archive collection.
exec 9>"$PRESENTATION_ROOT/.rail.lock"
if ! flock -x 9; then
  echo "presentation stage failed: could not acquire the presentation rail lock" >&2
  exit 1
fi

INCOMPLETE="$PRESENTATION_ROOT/$RUN_ID.incomplete"
FINAL="$PRESENTATION_ROOT/$RUN_ID"
if [ -e "$INCOMPLETE" ] || [ -e "$FINAL" ]; then
  echo "presentation stage failed: run ID already exists: $RUN_ID" >&2
  exit 1
fi
if ! mkdir -p "$INCOMPLETE"; then
  fail_stage "could not create the incomplete presentation directory"
fi

MANIFEST_TMP="$INCOMPLETE/presentation-manifest.txt.tmp"
MANIFEST="$INCOMPLETE/presentation-manifest.txt"
if ! printf 'presentation_version=1\nrun_id=%s\n' "$RUN_ID" >"$MANIFEST_TMP"; then
  fail_stage "could not start the presentation manifest"
fi

scenario_count=0
while IFS= read -r scenario || [ -n "$scenario" ]; do
  if [[ ! "$scenario" =~ ^[a-z0-9][a-z0-9-]*$ ]]; then
    fail_stage "invalid scenario registry entry: '$scenario'"
  fi
  if ! "$SCENARIO_RUNNER" "$scenario" </dev/null; then
    fail_stage "scenario failed: $scenario"
  fi

  source_capture="$PROJECT_DIR/build/mame-scenario/$scenario/$scenario.png"
  destination_name="$scenario.png"
  destination_capture="$INCOMPLETE/$destination_name"
  if [ ! -f "$source_capture" ]; then
    fail_stage "scenario did not finalize its capture: $source_capture"
  fi
  if ! cp -f "$source_capture" "$destination_capture"; then
    fail_stage "could not collect $source_capture"
  fi
  if ! digest="$(python3 -c 'import hashlib, pathlib, sys; print(hashlib.sha256(pathlib.Path(sys.argv[1]).read_bytes()).hexdigest())' "$destination_capture")"; then
    fail_stage "could not hash $destination_capture"
  fi
  if ! printf 'capture_sha256=%s  %s\n' "$digest" "$destination_name" >>"$MANIFEST_TMP"; then
    fail_stage "could not append $destination_name to the manifest"
  fi
  scenario_count=$((scenario_count + 1))
done <"$SCENARIO_REGISTRY"

if [ "$scenario_count" -eq 0 ]; then
  fail_stage "scenario registry is empty"
fi
if ! printf 'scenario_count=%d\nresult=passed\n' "$scenario_count" >>"$MANIFEST_TMP"; then
  fail_stage "could not finalize the presentation manifest"
fi
if ! mv "$MANIFEST_TMP" "$MANIFEST"; then
  fail_stage "could not commit the presentation manifest"
fi
if ! mv "$INCOMPLETE" "$FINAL"; then
  fail_stage "could not publish the completed presentation directory"
fi

echo "Toolbox presentation rail passed: $FINAL"
