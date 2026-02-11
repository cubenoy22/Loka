#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/scripts/macos/lib-common.sh"

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
  loka_cleanup_stale_output_dirs "${BUILD_DIR}"
  loka_build_requested_or_known_targets "${ROOT_DIR}"
done

loka_merge_requested_or_known_targets_multi_arch "${BUILD_ROOT}" "${BUILD_CFG}" "${OLD_ARCHS}"

if [[ -n "${OLD_TARGET}" ]]; then
  export TARGET="${OLD_TARGET}"
else
  unset TARGET || true
fi
export ARCHS="${OLD_ARCHS}"
