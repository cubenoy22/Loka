#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUBJECT="$PROJECT_DIR/tests/toolbox/verify-standalone-audit.sh"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

TUTORIAL_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/tutorial/increment-summary-toggle.audit"
HELLO_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/helloworld/toggle-action-probe.audit"
MINESWEEPER_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/minesweeper/new-game-twice.audit"
FLOPPY_BIRD_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/floppybird/fixed-step-flaps.audit"

"$SUBJECT" tutorial increment-summary-toggle "$TUTORIAL_EXPECTED" >/dev/null
"$SUBJECT" helloworld toggle-action-probe "$HELLO_EXPECTED" >/dev/null
"$SUBJECT" minesweeper new-game-twice "$MINESWEEPER_EXPECTED" >/dev/null
"$SUBJECT" floppybird fixed-step-flaps "$FLOPPY_BIRD_EXPECTED" >/dev/null

sed 's/text.value\tItems: 2/text.value\tItems: 1/' "$TUTORIAL_EXPECTED" >"$SANDBOX/mutated.audit"
if "$SUBJECT" tutorial increment-summary-toggle "$SANDBOX/mutated.audit" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: a verdict mutation passed" >&2
  exit 1
fi
sed 's/board.mines\t3,26,42,49,50,56,59,60,62,63/board.mines\t0,6,10,16,25,26,31,33,48,55/' \
  "$MINESWEEPER_EXPECTED" >"$SANDBOX/minesweeper-mutated.audit"
if "$SUBJECT" minesweeper new-game-twice "$SANDBOX/minesweeper-mutated.audit" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: a MineSweeper board mutation passed" >&2
  exit 1
fi
sed 's/checkpoint.first_pipe\t318,0,24,60/checkpoint.first_pipe\t318,0,24,39/' \
  "$FLOPPY_BIRD_EXPECTED" >"$SANDBOX/floppybird-mutated.audit"
if "$SUBJECT" floppybird fixed-step-flaps "$SANDBOX/floppybird-mutated.audit" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: a FloppyBird checkpoint mutation passed" >&2
  exit 1
fi
if "$SUBJECT" tutorial missing "$TUTORIAL_EXPECTED" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: an unregistered scenario passed" >&2
  exit 1
fi

echo "Toolbox standalone audit verifier tests passed"
