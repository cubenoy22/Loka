#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

export MAC_OS_10_4=0
export DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-11.0}"
export ARCHS="${ARCHS:-arm64;x86_64}"
export BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-ub2/${BUILD_TYPE:-Release}}"

if [[ -n "${TARGET:-}" ]]; then
  exec "${ROOT_DIR}/scripts/macos/build.sh"
fi

export TARGET="LokaHelloMacOS"
"${ROOT_DIR}/scripts/macos/build.sh"
export TARGET="LokaMineMacOS"
"${ROOT_DIR}/scripts/macos/build.sh"
export TARGET="LokaSimpleViewerMacOS"
exec "${ROOT_DIR}/scripts/macos/build.sh"
