#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.5}"
ARCHS="${ARCHS:-i386;x86_64;ppc;ppc64}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-xcodeproj-10.6-ub1/${BUILD_TYPE}}"
OSX_SYSROOT="${OSX_SYSROOT:-}"

if [[ -z "${OSX_SYSROOT}" ]]; then
  CANDIDATE_SYSROOTS=()
  if [[ -n "${DEVELOPER_DIR:-}" ]]; then
    CANDIDATE_SYSROOTS+=("${DEVELOPER_DIR}/SDKs/MacOSX10.6.sdk")
    CANDIDATE_SYSROOTS+=("${DEVELOPER_DIR}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk")
  fi
  if command -v xcode-select >/dev/null 2>&1; then
    XCODE_DEVELOPER_DIR="$(xcode-select -p 2>/dev/null || true)"
    if [[ -n "${XCODE_DEVELOPER_DIR}" ]]; then
      CANDIDATE_SYSROOTS+=("${XCODE_DEVELOPER_DIR}/SDKs/MacOSX10.6.sdk")
      CANDIDATE_SYSROOTS+=("${XCODE_DEVELOPER_DIR}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk")
    fi
  fi
  CANDIDATE_SYSROOTS+=("/Developer/SDKs/MacOSX10.6.sdk")
  CANDIDATE_SYSROOTS+=("/Applications/Xcode.app/Contents/Developer/SDKs/MacOSX10.6.sdk")
  CANDIDATE_SYSROOTS+=("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.6.sdk")

  for CANDIDATE_SYSROOT in "${CANDIDATE_SYSROOTS[@]}"; do
    if [[ -d "${CANDIDATE_SYSROOT}" ]]; then
      OSX_SYSROOT="${CANDIDATE_SYSROOT}"
      break
    fi
  done
fi

if [[ -z "${OSX_SYSROOT}" || ! -d "${OSX_SYSROOT}" ]]; then
  echo "error: MacOSX10.6.sdk not found." >&2
  echo "Set OSX_SYSROOT to your MacOSX10.6.sdk path and retry." >&2
  exit 1
fi

if command -v xcodebuild >/dev/null 2>&1; then
  XCODE_VERSION="$(xcodebuild -version 2>/dev/null | awk '/^Xcode / {print $2; exit}' || true)"
  if [[ -z "${XCODE_VERSION}" ]]; then
    echo "error: unable to read Xcode version from xcodebuild." >&2
    echo "A full Xcode.app installation is required for the Xcode generator." >&2
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
  "-DCMAKE_OSX_ARCHITECTURES=${ARCHS}"
  "-DLOKA_MACOS_EXPLICIT_NO_ARC_FLAGS=OFF"
  "-DCMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC=NO"
)

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
echo "[gen-xcodeproj-10_6-ub1] running: cmake ${CMAKE_ARGS[*]}"
cmake "${CMAKE_ARGS[@]}"

echo "Generated 10.6 UB1 Xcode project in:"
echo "  ${BUILD_DIR}"
echo "SDK: ${OSX_SYSROOT}"
echo "Archs: ${ARCHS}"
echo "Deployment target: ${DEPLOYMENT_TARGET}"
