#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
# Default to Xcode's standard architecture preset so modern Xcode versions can
# choose the right host/default universal set. Override this when you need an
# explicit arch list, for example:
#   ARCHS="arm64;x86_64" DEPLOYMENT_TARGET=11.0 ./scripts/macos/gen-xcodeproj.sh
# For older single-arch experiments, for example:
#   ARCHS="i386" DEPLOYMENT_TARGET=10.5 ./scripts/macos/gen-xcodeproj.sh
ARCH_MODE="${ARCHS:-xcode-standard}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-11}"
OSX_SYSROOT="${OSX_SYSROOT:-macosx}"
CMAKE_OSX_ARCHITECTURES_VALUE="${ARCH_MODE}"
XCODE_ARCHS="${XCODE_ARCHS:-}"
DISPLAY_ARCHS="${ARCH_MODE}"

case "${ARCH_MODE}" in
  xcode-standard|archs-standard)
    ARCH_MODE="xcode-standard"
    DISPLAY_ARCHS='$(ARCHS_STANDARD)'
    CMAKE_OSX_ARCHITECTURES_VALUE=""
    XCODE_ARCHS='$(ARCHS_STANDARD)'
    ;;
esac

BUILD_VARIANT="${ARCH_MODE//[^A-Za-z0-9_.-]/-}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-xcodeproj/${BUILD_TYPE}-${BUILD_VARIANT}}"

echo "[gen-xcodeproj] ROOT_DIR=${ROOT_DIR}"
echo "[gen-xcodeproj] BUILD_DIR=${BUILD_DIR}"
echo "[gen-xcodeproj] ARCH_MODE=${ARCH_MODE} ARCHS=${DISPLAY_ARCHS} DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
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
else
  CMAKE_ARGS+=("-U" "CMAKE_OSX_ARCHITECTURES")
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

CMAKE_ARGS+=("-DCMAKE_OSX_SYSROOT=${OSX_SYSROOT}")

echo "[gen-xcodeproj] running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo "Generated Xcode project in:"
echo "  ${BUILD_DIR}"
echo "Arch mode: ${ARCH_MODE}"
echo "Archs: ${DISPLAY_ARCHS}"
if [[ -n "${XCODE_ARCHS}" ]]; then
  echo "Xcode ARCHS: ${XCODE_ARCHS}"
fi
echo "Deployment target: ${DEPLOYMENT_TARGET}"
