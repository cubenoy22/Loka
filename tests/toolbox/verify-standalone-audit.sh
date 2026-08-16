#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

usage() {
  echo "Usage: $0 <example> <scenario from scenarios.txt> <LOG.TXT>" >&2
}

if [ "$#" -ne 3 ]; then
  usage
  exit 2
fi

EXAMPLE="$1"
SCENARIO="$2"
ACTUAL="$3"
if [[ ! "$EXAMPLE" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || [[ ! "$SCENARIO" =~ ^[a-z0-9][a-z0-9-]*$ ]] \
  || ! grep -Fxq -- "$EXAMPLE $SCENARIO" "$PROJECT_DIR/tests/scenarios/scenarios.txt"; then
  usage
  exit 2
fi

EXPECTED="$PROJECT_DIR/tests/scenarios/expected/$EXAMPLE/$SCENARIO.audit"
if [ ! -f "$EXPECTED" ]; then
  echo "Tracked expected audit not found: $EXPECTED" >&2
  exit 1
fi
if [ ! -f "$ACTUAL" ]; then
  echo "Standalone audit not found: $ACTUAL" >&2
  exit 1
fi
if ! cmp "$EXPECTED" "$ACTUAL"; then
  echo "Standalone audit does not match the tracked expected audit: $EXPECTED" >&2
  exit 1
fi

echo "Standalone audit passed: $EXAMPLE/$SCENARIO"
