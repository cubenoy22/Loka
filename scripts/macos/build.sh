#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos/${BUILD_TYPE}}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.7}"
GENERATOR="${GENERATOR:-}"
ARCHS="${ARCHS:-}"
TARGET="${TARGET:-}"
MAC_OS_10_4="${MAC_OS_10_4:-${MAC_LEGACY:-0}}"
MAC_OS_10_4_SYSROOT="${MAC_OS_10_4_SYSROOT:-${LEGACY_SYSROOT:-}}"

if [[ -z "${GENERATOR}" ]]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
  else
    GENERATOR="Unix Makefiles"
  fi
fi

CMAKE_ARGS=(
  "-S" "${ROOT_DIR}"
  "-B" "${BUILD_DIR}"
  "-G" "${GENERATOR}"
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
)

if [[ -n "${ARCHS}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=${ARCHS}")
fi

if [[ "${MAC_OS_10_4}" == "1" ]]; then
  CMAKE_ARGS+=("-DLOKA_MAC_OS_10_4=ON")
  if [[ -n "${MAC_OS_10_4_SYSROOT}" ]]; then
    CMAKE_ARGS+=("-DLOKA_MAC_OS_10_4_SYSROOT=${MAC_OS_10_4_SYSROOT}")
  fi
fi

cmake "${CMAKE_ARGS[@]}"

if [[ -n "${TARGET}" ]]; then
  cmake --build "${BUILD_DIR}" --target "${TARGET}"
else
  cmake --build "${BUILD_DIR}"
fi
