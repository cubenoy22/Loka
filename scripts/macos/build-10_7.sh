#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

export MAC_OS_10_4=0
export DEPLOYMENT_TARGET=10.7
export BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-10.7/${BUILD_TYPE:-Release}}"

exec "${ROOT_DIR}/scripts/macos/build.sh"
