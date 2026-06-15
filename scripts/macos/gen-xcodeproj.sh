#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
# Default to a conservative Xcode-project configuration that is easier to
# open across older Apple IDEs. Override these when you want a more modern
# project shape, for example:
#   ARCHS=xcode-standard DEPLOYMENT_TARGET=11.0 ./scripts/macos/gen-xcodeproj.sh
# For older single-arch experiments, for example:
#   ARCHS="i386" DEPLOYMENT_TARGET=10.5 ./scripts/macos/gen-xcodeproj.sh
ARCHS="${ARCHS:-x86_64}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.8}"
OSX_SYSROOT="${OSX_SYSROOT:-}"
CMAKE_OSX_ARCHITECTURES_VALUE="${ARCHS}"
XCODE_ARCHS="${XCODE_ARCHS:-}"

case "${ARCHS}" in
  xcode-standard)
    ARCHS='$(ARCHS_STANDARD)'
    CMAKE_OSX_ARCHITECTURES_VALUE=""
    XCODE_ARCHS='$(ARCHS_STANDARD)'
    ;;
  native64)
    ARCHS="x86_64"
    CMAKE_OSX_ARCHITECTURES_VALUE="${ARCHS}"
    ;;
  ub2)
    ARCHS="arm64;x86_64"
    CMAKE_OSX_ARCHITECTURES_VALUE="${ARCHS}"
    ;;
esac

BUILD_VARIANT="${ARCHS//[^A-Za-z0-9_.-]/-}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-xcodeproj/${BUILD_TYPE}-${BUILD_VARIANT}}"

echo "[gen-xcodeproj] ROOT_DIR=${ROOT_DIR}"
echo "[gen-xcodeproj] BUILD_DIR=${BUILD_DIR}"
echo "[gen-xcodeproj] ARCHS=${ARCHS} DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
if [[ -n "${XCODE_ARCHS}" ]]; then
  echo "[gen-xcodeproj] XCODE_ARCHS=${XCODE_ARCHS}"
fi

if command -v xcodebuild >/dev/null 2>&1; then
  XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}' || true)"
  if [[ -z "${XCODE_VERSION}" ]]; then
    echo "error: unable to read Xcode version from xcodebuild." >&2
    echo "gen-xcodeproj.sh requires a full Xcode.app installation, not just Command Line Tools." >&2
    echo "Run 'xcode-select -p' and ensure xcodebuild points to a real Xcode installation." >&2
    exit 1
  fi
  echo "[gen-xcodeproj] xcodebuild version=${XCODE_VERSION:-unknown}"
fi

if ! cmake --help 2>/dev/null | grep -q "Xcode"; then
  echo "error: this CMake build does not provide the Xcode generator." >&2
  echo "Install a CMake build with Xcode generator support." >&2
  exit 1
fi

CMAKE_ARGS=(
  "-S" "${ROOT_DIR}"
  "-B" "${BUILD_DIR}"
  "-G" "Xcode"
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
)

if [[ -n "${CMAKE_OSX_ARCHITECTURES_VALUE}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES_VALUE}")
fi

if [[ -n "${XCODE_ARCHS}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_ARCHS=${XCODE_ARCHS}")
fi

if [[ -n "${CC:-}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_C_COMPILER=${CC}")
fi

if [[ -n "${CXX:-}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=${CXX}")
fi

if [[ -n "${OSX_SYSROOT}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=${OSX_SYSROOT}")
fi

echo "[gen-xcodeproj] running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo "Generated Xcode project in:"
echo "  ${BUILD_DIR}"
echo "Archs: ${ARCHS}"
if [[ -n "${XCODE_ARCHS}" ]]; then
  echo "Xcode ARCHS: ${XCODE_ARCHS}"
fi
echo "Deployment target: ${DEPLOYMENT_TARGET}"
