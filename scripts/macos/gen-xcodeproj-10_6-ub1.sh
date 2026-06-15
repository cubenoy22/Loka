#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.5}"
ARCHS="${ARCHS:-xcode-standard-32-64}"
BUILD_VARIANT="${ARCHS//[^A-Za-z0-9_.-]/-}"
CMAKE_OSX_ARCHITECTURES_VALUE="${ARCHS}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-xcodeproj-10.6-ub1/${BUILD_TYPE}-${BUILD_VARIANT}}"
OSX_SYSROOT="${OSX_SYSROOT:-macosx}"
XCODE_ARCHS="${XCODE_ARCHS:-}"
XCODE_VALID_ARCHS="${XCODE_VALID_ARCHS:-}"

case "${ARCHS}" in
  xcode-standard-32-64)
    ARCHS='$(ARCHS_STANDARD_32_64_BIT)'
    CMAKE_OSX_ARCHITECTURES_VALUE=""
    XCODE_ARCHS='$(ARCHS_STANDARD_32_64_BIT)'
    ;;
esac

if command -v xcodebuild >/dev/null 2>&1; then
  XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}' || true)"
  if [[ -z "${XCODE_VERSION}" ]]; then
    echo "error: unable to read Xcode version from xcodebuild." >&2
    echo "A full Xcode.app installation is required for the Xcode generator." >&2
    exit 1
  fi
  XCODE_MAJOR="${XCODE_VERSION%%.*}"
  if [[ "${XCODE_MAJOR}" =~ ^[0-9]+$ && "${XCODE_MAJOR}" -lt 5 ]]; then
    echo "error: CMake's Xcode generator requires Xcode 5.0 or newer." >&2
    echo "Generate on a newer Mac, then copy the project to Snow Leopard for Xcode 3.2.6 testing." >&2
    exit 1
  fi
  echo "[gen-xcodeproj-10_6-ub1] xcodebuild version=${XCODE_VERSION}"
fi

if ! cmake --help 2>/dev/null | grep -q "Xcode"; then
  echo "error: this CMake build does not provide the Xcode generator." >&2
  exit 1
fi

CMAKE_ARGS=(
  "-S" "${ROOT_DIR}"
  "-B" "${BUILD_DIR}"
  "-G" "Xcode"
  "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
  "-DCMAKE_OSX_SYSROOT=${OSX_SYSROOT}"
  "-DCMAKE_OSX_DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
  "-DCMAKE_SUPPRESS_REGENERATION=ON"
  "-DLOKA_MACOS_EXPLICIT_NO_ARC_FLAGS=OFF"
  "-DLOKA_MACOS_SUPPRESS_ARCLITE_LINK=ON"
  "-DCMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=NO"
  "-DCMAKE_XCODE_ATTRIBUTE_CLANG_LINK_OBJC_RUNTIME=NO"
  "-DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO"
)

if [[ -n "${CMAKE_OSX_ARCHITECTURES_VALUE}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES_VALUE}")
else
  CMAKE_ARGS+=("-U" "CMAKE_OSX_ARCHITECTURES")
fi

if [[ -n "${XCODE_ARCHS}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_ARCHS=${XCODE_ARCHS}")
fi

if [[ -n "${XCODE_VALID_ARCHS}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_XCODE_ATTRIBUTE_VALID_ARCHS=${XCODE_VALID_ARCHS}")
fi

if [[ -n "${CC:-}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_C_COMPILER=${CC}")
fi

if [[ -n "${CXX:-}" ]]; then
  CMAKE_ARGS+=("-DCMAKE_CXX_COMPILER=${CXX}")
fi

echo "[gen-xcodeproj-10_6-ub1] ROOT_DIR=${ROOT_DIR}"
echo "[gen-xcodeproj-10_6-ub1] BUILD_DIR=${BUILD_DIR}"
echo "[gen-xcodeproj-10_6-ub1] SDK=${OSX_SYSROOT}"
echo "[gen-xcodeproj-10_6-ub1] ARCHS=${ARCHS} DEPLOYMENT_TARGET=${DEPLOYMENT_TARGET}"
if [[ -n "${XCODE_ARCHS}" ]]; then
  echo "[gen-xcodeproj-10_6-ub1] XCODE_ARCHS=${XCODE_ARCHS}"
fi
if [[ -n "${XCODE_VALID_ARCHS}" ]]; then
  echo "[gen-xcodeproj-10_6-ub1] XCODE_VALID_ARCHS=${XCODE_VALID_ARCHS}"
fi
echo "[gen-xcodeproj-10_6-ub1] running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo "Generated 10.6 UB1 Xcode project in:"
echo "  ${BUILD_DIR}"
echo "SDK: ${OSX_SYSROOT}"
echo "Archs: ${ARCHS}"
if [[ -n "${XCODE_ARCHS}" ]]; then
  echo "Xcode ARCHS: ${XCODE_ARCHS}"
fi
echo "Deployment target: ${DEPLOYMENT_TARGET}"
