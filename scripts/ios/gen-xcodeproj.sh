#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/scripts/apple/lib-xcode.sh"

BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/ios-xcodeproj/${BUILD_TYPE}}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-}"
IOS_SYSROOT="${IOS_SYSROOT:-}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: iOS Xcode projects must be generated on macOS." >&2
  exit 1
fi

loka_require_full_xcode_for_generator "gen-ios-xcodeproj"
loka_require_cmake_xcode_generator

CMAKE_ARGS=(
  "-S" "${ROOT_DIR}"
  "-B" "${BUILD_DIR}"
  "-G" "Xcode"
  "-DCMAKE_SYSTEM_NAME=iOS"
  "-DLOKA_ENABLE_APPLE=ON"
  "-DLOKA_WARNINGS_AS_ERRORS=ON"
  "-DTEST_BUILD=OFF"
)

if [[ -n "${DEPLOYMENT_TARGET}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}")
fi

if [[ -n "${IOS_SYSROOT}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=${IOS_SYSROOT}")
fi

echo "[gen-ios-xcodeproj] ROOT_DIR=${ROOT_DIR}"
echo "[gen-ios-xcodeproj] BUILD_DIR=${BUILD_DIR}"
if [[ -n "${DEPLOYMENT_TARGET}" ]]; then
  echo "[gen-ios-xcodeproj] DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
else
  echo "[gen-ios-xcodeproj] DEPLOYMENT_TARGET=Xcode default"
fi
if [[ -n "${IOS_SYSROOT}" ]]; then
  echo "[gen-ios-xcodeproj] IOS_SYSROOT=${IOS_SYSROOT}"
else
  echo "[gen-ios-xcodeproj] IOS_SYSROOT=CMake device default"
fi
echo "[gen-ios-xcodeproj] running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo "Generated Xcode project in:"
echo "  ${BUILD_DIR}"
echo "Target: LokaUIKitHelloWorld (iPhone and iPad)"
