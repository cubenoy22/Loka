#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

export MAC_OS_10_4=0
export DEPLOYMENT_TARGET="${DEPLOYMENT_TARGET:-10.5}"
export ARCHS="${ARCHS:-ppc;i386;x86_64}"
export OSX_SYSROOT="${OSX_SYSROOT:-/Developer/SDKs/MacOSX10.5.sdk}"
if command -v gcc-4.2 >/dev/null 2>&1; then
  export CC="${CC:-$(command -v gcc-4.2)}"
fi
if command -v g++-4.2 >/dev/null 2>&1; then
  export CXX="${CXX:-$(command -v g++-4.2)}"
fi

if [[ "${ARCHS}" != *";"* ]]; then
  echo "error: scripts/macos/build-10_5.sh expects multi-arch ARCHS (e.g. ppc;i386;x86_64)." >&2
  echo "Use scripts/macos/build.sh directly for single-arch builds." >&2
  exit 1
fi

if [[ "${ARCHS}" == *"ppc64"* ]]; then
  echo "error: ppc64 is intentionally excluded from scripts/macos/build-10_5.sh." >&2
  echo "Use ARCHS without ppc64 (default: ppc;i386;x86_64)." >&2
  exit 1
fi

OLD_ARCHS="${ARCHS}"
OLD_TARGET="${TARGET:-}"
IFS=';' read -r -a ARCH_LIST <<< "${ARCHS}"
BUILD_ROOT="${ROOT_DIR}/build/macos-10.5-ub1"
BUILD_CFG="${BUILD_TYPE:-Release}"

for ARCH in "${ARCH_LIST[@]}"; do
  export ARCHS="${ARCH}"
  export BUILD_DIR="${BUILD_ROOT}/${BUILD_CFG}-${ARCH}"
  if [[ -n "${OLD_TARGET}" ]]; then
    export TARGET="${OLD_TARGET}"
    "${ROOT_DIR}/scripts/macos/build.sh"
  else
    export TARGET="LokaHelloMacOS"
    "${ROOT_DIR}/scripts/macos/build.sh"
    export TARGET="LokaMineMacOS"
    "${ROOT_DIR}/scripts/macos/build.sh"
    export TARGET="LokaSimpleViewerMacOS"
    "${ROOT_DIR}/scripts/macos/build.sh"
  fi
done

mkdir -p "${BUILD_ROOT}/universal"

merge_one() {
  local rel_path="$1"
  local out_bin="${BUILD_ROOT}/universal/$(basename "${rel_path}")"
  local inputs=()
  local bin
  local arch
  for arch in "${ARCH_LIST[@]}"; do
    bin="${BUILD_ROOT}/${BUILD_CFG}-${arch}/${rel_path}"
    if [[ -f "${bin}" ]]; then
      inputs+=("${bin}")
    fi
  done
  if [[ "${#inputs[@]}" -ge 2 ]]; then
    lipo -create "${inputs[@]}" -output "${out_bin}"
    lipo -info "${out_bin}"
  elif [[ "${#inputs[@]}" -eq 1 ]]; then
    cp -f "${inputs[0]}" "${out_bin}"
  fi
}

if [[ -n "${OLD_TARGET}" ]]; then
  case "${OLD_TARGET}" in
    LokaHelloMacOS) merge_one "example/HelloWorld/LokaHelloMacOS" ;;
    LokaMineMacOS) merge_one "example/MineSweeper/LokaMineMacOS" ;;
    LokaSimpleViewerMacOS) merge_one "example/SimpleViewer/LokaSimpleViewerMacOS" ;;
  esac
else
  merge_one "example/HelloWorld/LokaHelloMacOS"
  merge_one "example/MineSweeper/LokaMineMacOS"
  merge_one "example/SimpleViewer/LokaSimpleViewerMacOS"
fi

if [[ -n "${OLD_TARGET}" ]]; then
  export TARGET="${OLD_TARGET}"
else
  unset TARGET || true
fi
export ARCHS="${OLD_ARCHS}"
