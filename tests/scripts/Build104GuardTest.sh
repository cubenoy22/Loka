#!/usr/bin/env bash

# Pins build-10_4.sh's DEVELOPER_DIR guard (#198) from both sides, without a
# Mac: the refusal must name the missing xcrun and exit before any toolchain
# work, and a DEVELOPER_DIR that does carry xcrun must pass through to the
# pre-existing toolchain check. CC/CXX are pinned to /bin/false so the
# pass-through leg fails at that check deterministically on every host --
# which is exactly the discrimination: deleting the guard turns the refusal
# leg's message into the toolchain error, and an over-eager guard turns the
# pass-through leg's toolchain error into the xcrun refusal.

set -euo pipefail

TEST_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$TEST_SCRIPT_DIR/../.." && pwd)"
SUBJECT="${LOKA_BUILD_10_4_SH:-$REPO_DIR/scripts/macos/build-10_4.sh}"

SANDBOX="$(mktemp -d)"
cleanup() { rm -rf "$SANDBOX"; }
trap cleanup EXIT

fail() {
  echo "Build104GuardTest: $1" >&2
  exit 1
}

mkdir -p "$SANDBOX/no-xcrun/usr/bin" "$SANDBOX/with-xcrun/usr/bin"
printf '#!/bin/sh\nexit 0\n' > "$SANDBOX/with-xcrun/usr/bin/xcrun"
chmod +x "$SANDBOX/with-xcrun/usr/bin/xcrun"

# Refusal leg: no usr/bin/xcrun under DEVELOPER_DIR.
set +e
DEVELOPER_DIR="$SANDBOX/no-xcrun" CC=/bin/false CXX=/bin/false ARCHS=ppc \
  bash "$SUBJECT" > "$SANDBOX/refuse.out" 2>&1
REFUSE_EXIT=$?
set -e
[ "$REFUSE_EXIT" -ne 0 ] || fail "refusal leg exited 0"
grep -q "has no usr/bin/xcrun" "$SANDBOX/refuse.out" \
  || fail "refusal leg did not name the missing xcrun: $(cat "$SANDBOX/refuse.out")"
grep -q "ppc toolchain check failed" "$SANDBOX/refuse.out" \
  && fail "refusal leg reached the toolchain check; the guard did not fire"

# Pass-through leg: xcrun present, so the guard must step aside and the
# script must die later, at the pinned-to-/bin/false toolchain check.
set +e
DEVELOPER_DIR="$SANDBOX/with-xcrun" CC=/bin/false CXX=/bin/false ARCHS=ppc \
  bash "$SUBJECT" > "$SANDBOX/pass.out" 2>&1
PASS_EXIT=$?
set -e
[ "$PASS_EXIT" -ne 0 ] || fail "pass-through leg exited 0 with /bin/false compilers"
grep -q "has no usr/bin/xcrun" "$SANDBOX/pass.out" \
  && fail "guard refused a DEVELOPER_DIR that has xcrun: $(cat "$SANDBOX/pass.out")"
grep -q "ppc toolchain check failed" "$SANDBOX/pass.out" \
  || fail "pass-through leg did not reach the toolchain check: $(cat "$SANDBOX/pass.out")"

echo "Build104GuardTest passed"
