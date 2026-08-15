#!/usr/bin/env bash

set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SUBJECT="$PROJECT_DIR/tests/toolbox/verify-standalone-audit.sh"
SANDBOX="$(mktemp -d)"
trap 'rm -rf "$SANDBOX"' EXIT

TUTORIAL_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/tutorial/increment-summary-toggle.audit"
HELLO_EXPECTED="$PROJECT_DIR/tests/scenarios/expected/helloworld/toggle-action-probe.audit"

"$SUBJECT" tutorial increment-summary-toggle "$TUTORIAL_EXPECTED" >/dev/null
"$SUBJECT" helloworld toggle-action-probe "$HELLO_EXPECTED" >/dev/null

sed 's/text.value\tItems: 2/text.value\tItems: 1/' "$TUTORIAL_EXPECTED" >"$SANDBOX/mutated.audit"
if "$SUBJECT" tutorial increment-summary-toggle "$SANDBOX/mutated.audit" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: a verdict mutation passed" >&2
  exit 1
fi
if "$SUBJECT" tutorial missing "$TUTORIAL_EXPECTED" >/dev/null 2>&1; then
  echo "ToolboxStandaloneAuditVerifierTest failed: an unregistered scenario passed" >&2
  exit 1
fi

echo "Toolbox standalone audit verifier tests passed"
