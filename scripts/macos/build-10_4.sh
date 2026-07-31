#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${ROOT_DIR}/scripts/macos/lib-common.sh"

# A DEVELOPER_DIR that has no xcrun (Xcode 3.2.6 predates it) compiles all the
# way to linking and then dies in an auxiliary xcrun step with a confusing
# error (#198) -- demonstrated on Mavericks, whose modern xcrun requires
# $DEVELOPER_DIR/usr/bin/xcrun to exist. Refuse up front with the fix spelled
# out; a DEVELOPER_DIR that does carry xcrun is left alone. Whether an old
# host's own xcrun tolerates such a DEVELOPER_DIR is unverified (no Snow
# Leopard run yet), so the refusal has the same escape hatch shape as
# ALLOW_PPC_FALLBACK for a host where the configuration is known to work.
if [[ -n "${DEVELOPER_DIR:-}" && ! -x "${DEVELOPER_DIR}/usr/bin/xcrun" ]]; then
  if [[ "${ALLOW_DEVELOPER_DIR_WITHOUT_XCRUN:-0}" != "1" ]]; then
    echo "error: DEVELOPER_DIR=${DEVELOPER_DIR} has no usr/bin/xcrun (Xcode 3.2.6 predates xcrun)." >&2
    echo "On hosts with a modern xcrun the build fails at link time. unset DEVELOPER_DIR and" >&2
    echo "select the legacy tools directly instead, e.g.:" >&2
    echo "  unset DEVELOPER_DIR" >&2
    echo "  export MAC_OS_10_4_SYSROOT=/Developer/SDKs/MacOSX10.4u.sdk" >&2
    echo "  export CC=/Developer/usr/bin/gcc-4.0 CXX=/Developer/usr/bin/g++-4.0" >&2
    echo "If this configuration is known to work on this host, set" >&2
    echo "ALLOW_DEVELOPER_DIR_WITHOUT_XCRUN=1 to proceed." >&2
    exit 1
  fi
  echo "warning: proceeding with DEVELOPER_DIR=${DEVELOPER_DIR} despite missing usr/bin/xcrun" >&2
  echo "(ALLOW_DEVELOPER_DIR_WITHOUT_XCRUN=1); a modern xcrun would fail at link time." >&2
fi

export MAC_OS_10_4=1
export DEPLOYMENT_TARGET=10.4
export ARCHS="${ARCHS:-ppc;i386}"

if [[ -z "${MAC_OS_10_4_SYSROOT:-}" ]]; then
  MAC_OS_10_4_SYSROOT="$(loka_find_selected_sdk "MacOSX10.4u.sdk" || true)"
  export MAC_OS_10_4_SYSROOT
fi

export MAC_OS_10_4_SYSROOT="${MAC_OS_10_4_SYSROOT:-/Developer/SDKs/MacOSX10.4u.sdk}"
export OSX_SYSROOT="${OSX_SYSROOT:-${MAC_OS_10_4_SYSROOT}}"
if [[ -z "${CC:-}" ]]; then
  CC_CANDIDATE="$(loka_find_first_selected_tool gcc-4.0 gcc-4.2 || true)"
  if [[ -n "${CC_CANDIDATE}" ]]; then
    export CC="${CC_CANDIDATE}"
  fi
fi
if [[ -z "${CXX:-}" ]]; then
  CXX_CANDIDATE="$(loka_find_first_selected_tool g++-4.0 g++-4.2 || true)"
  if [[ -n "${CXX_CANDIDATE}" ]]; then
    export CXX="${CXX_CANDIDATE}"
  fi
fi

if [[ "${ARCHS}" == *"ppc"* ]]; then
  CXX_FOR_PPC_CHECK="${CXX:-$(loka_find_first_selected_tool g++-4.0 g++-4.2 g++ || true)}"
  PPC_TOOLCHAIN_OK=0
  if [[ -n "${CXX_FOR_PPC_CHECK}" ]]; then
    PPC_TEST_CXX="$(mktemp /tmp/loka-ppc-check-XXXXXX.cpp)"
    PPC_TEST_OBJ="$(mktemp /tmp/loka-ppc-check-XXXXXX.o)"
    echo "int main() { return 0; }" > "${PPC_TEST_CXX}"
    if "${CXX_FOR_PPC_CHECK}" -arch ppc -c "${PPC_TEST_CXX}" -o "${PPC_TEST_OBJ}" >/dev/null 2>&1; then
      PPC_TOOLCHAIN_OK=1
    fi
    rm -f "${PPC_TEST_CXX}" "${PPC_TEST_OBJ}"
  fi

  if [[ "${PPC_TOOLCHAIN_OK}" != "1" ]]; then
    if [[ "${ALLOW_PPC_FALLBACK:-0}" != "1" ]]; then
      echo "error: ppc toolchain check failed for compiler '${CXX_FOR_PPC_CHECK}'." >&2
      echo "PPC build is required by default; install/select PPC-capable tools." >&2
      echo "If you intentionally want non-ppc fallback, set ALLOW_PPC_FALLBACK=1." >&2
      exit 1
    fi

    OLD_ARCHS="${ARCHS}"
    IFS=';' read -r -a ARCH_LIST_NO_PPC <<< "${ARCHS}"
    FILTERED_ARCHS=""
    for ARCH_CANDIDATE in "${ARCH_LIST_NO_PPC[@]}"; do
      if [[ "${ARCH_CANDIDATE}" != "ppc" ]]; then
        if [[ -z "${FILTERED_ARCHS}" ]]; then
          FILTERED_ARCHS="${ARCH_CANDIDATE}"
        else
          FILTERED_ARCHS="${FILTERED_ARCHS};${ARCH_CANDIDATE}"
        fi
      fi
    done

    if [[ -z "${FILTERED_ARCHS}" ]]; then
      echo "error: ppc tools are unavailable and no non-ppc arch remains in ARCHS=${OLD_ARCHS}." >&2
      echo "Install/select PPC tools or set ARCHS to a supported non-ppc value." >&2
      exit 1
    fi

    export ARCHS="${FILTERED_ARCHS}"
    echo "warning: ppc toolchain unavailable; falling back ARCHS from ${OLD_ARCHS} to ${ARCHS}." >&2
    echo "Set ALLOW_PPC_FALLBACK=0 (default) to fail instead of fallback." >&2
  fi
fi

if [[ "${ARCHS}" == *";"* ]]; then
  OLD_ARCHS="${ARCHS}"
  IFS=';' read -r -a ARCH_LIST <<< "${ARCHS}"
  BUILD_ROOT="${ROOT_DIR}/build/macos-10.4-ub1"
  BUILD_CFG="${BUILD_TYPE:-Release}"
  for ARCH in "${ARCH_LIST[@]}"; do
    export ARCHS="${ARCH}"
    export BUILD_DIR="${ROOT_DIR}/build/macos-10.4-ub1/${BUILD_TYPE:-Release}-${ARCH}"
    loka_cleanup_stale_output_dirs "${BUILD_DIR}"
    loka_build_requested_or_known_targets "${ROOT_DIR}"
  done

  if [[ "${#ARCH_LIST[@]}" -eq 2 && "${ARCH_LIST[0]}" == "ppc" && "${ARCH_LIST[1]}" == "i386" ]]; then
    loka_merge_requested_or_known_targets_two_arch "${BUILD_ROOT}" "${BUILD_CFG}" "ppc" "i386"
  fi

  export ARCHS="${OLD_ARCHS}"
else
  export BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/macos-10.4-ub1/${BUILD_TYPE:-Release}-${ARCHS}}"
  loka_cleanup_stale_output_dirs "${BUILD_DIR}"
  exec "${ROOT_DIR}/scripts/macos/build.sh"
fi
