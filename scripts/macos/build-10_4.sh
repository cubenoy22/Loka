#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

export MAC_OS_10_4=1
export DEPLOYMENT_TARGET=10.4
export ARCHS="${ARCHS:-ppc;i386}"
export MAC_OS_10_4_SYSROOT="${MAC_OS_10_4_SYSROOT:-/Developer/SDKs/MacOSX10.4u.sdk}"
if command -v gcc-4.0 >/dev/null 2>&1; then
  export CC="${CC:-$(command -v gcc-4.0)}"
fi
if command -v g++-4.0 >/dev/null 2>&1; then
  export CXX="${CXX:-$(command -v g++-4.0)}"
fi

if [[ "${ARCHS}" == *";"* ]]; then
  OLD_ARCHS="${ARCHS}"
  IFS=';' read -r -a ARCH_LIST <<< "${ARCHS}"
  BUILD_ROOT="${ROOT_DIR}/build/macos-10.4"
  BUILD_CFG="${BUILD_TYPE:-Release}"
  for ARCH in "${ARCH_LIST[@]}"; do
    export ARCHS="${ARCH}"
    export BUILD_DIR="${ROOT_DIR}/build/macos-10.4/${BUILD_TYPE:-Release}-${ARCH}"
    "${ROOT_DIR}/scripts/macos/build.sh"
  done

  if [[ "${#ARCH_LIST[@]}" -eq 2 && "${ARCH_LIST[0]}" == "ppc" && "${ARCH_LIST[1]}" == "i386" ]]; then
    mkdir -p "${BUILD_ROOT}/universal"

    merge_one() {
      local rel_path="$1"
      local ppc_bin="${BUILD_ROOT}/${BUILD_CFG}-ppc/${rel_path}"
      local i386_bin="${BUILD_ROOT}/${BUILD_CFG}-i386/${rel_path}"
      local out_bin="${BUILD_ROOT}/universal/$(basename "${rel_path}")"
      if [[ -f "${ppc_bin}" && -f "${i386_bin}" ]]; then
        lipo -create "${ppc_bin}" "${i386_bin}" -output "${out_bin}"
        lipo -info "${out_bin}"
      fi
    }

    if [[ -n "${TARGET:-}" ]]; then
      case "${TARGET}" in
        LokaHelloMacOS) merge_one "example/HelloWorld/LokaHelloMacOS" ;;
        LokaMineMacOS) merge_one "example/MineSweeper/LokaMineMacOS" ;;
        LokaSimpleViewerMacOS) merge_one "example/SimpleViewer/LokaSimpleViewerMacOS" ;;
      esac
    else
      merge_one "example/HelloWorld/LokaHelloMacOS"
      merge_one "example/MineSweeper/LokaMineMacOS"
      merge_one "example/SimpleViewer/LokaSimpleViewerMacOS"
    fi
  fi

  export ARCHS="${OLD_ARCHS}"
else
  export BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-10.4/${BUILD_TYPE:-Release}-${ARCHS}}"
  exec "${ROOT_DIR}/scripts/macos/build.sh"
fi
