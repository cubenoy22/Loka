#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
ARCHS="${ARCHS:-x86_64}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.8}"
OSX_SYSROOT="${OSX_SYSROOT:-}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-xcodeproj/${BUILD_TYPE}}"

echo "[gen-xcodeproj] ROOT_DIR=${ROOT_DIR}"
echo "[gen-xcodeproj] BUILD_DIR=${BUILD_DIR}"
echo "[gen-xcodeproj] ARCHS=${ARCHS} DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"

if command -v xcodebuild >/dev/null 2>&1; then
  XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}' || true)"
  if [[ -z "${XCODE_VERSION}" ]]; then
    echo "error: unable to read Xcode version from xcodebuild." >&2
    echo "Run 'xcode-select -p' and ensure Command Line Tools / Xcode is configured." >&2
    exit 1
  fi
  XCODE_MAJOR="${XCODE_VERSION%%.*}"
  echo "[gen-xcodeproj] xcodebuild version=${XCODE_VERSION:-unknown}"
  if [[ -n "${XCODE_MAJOR}" ]] && [[ "${XCODE_MAJOR}" -lt 4 ]]; then
    echo "error: CMake does not support generating Xcode projects for Xcode ${XCODE_VERSION}." >&2
    echo "Use wrapper build scripts with Makefiles/Ninja instead:" >&2
    echo "  ./scripts/macos/build-10_4.sh" >&2
    echo "  ./scripts/macos/build-10_5.sh" >&2
    echo "For custom single-arch builds, use:" >&2
    echo "  GENERATOR=\"Unix Makefiles\" ARCHS=\"x86_64\" DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET} ./scripts/macos/build.sh" >&2
    exit 1
  fi
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
  "-DCMAKE_OSX_ARCHITECTURES=${ARCHS}"
)

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
echo "Deployment target: ${DEPLOYMENT_TARGET}"
