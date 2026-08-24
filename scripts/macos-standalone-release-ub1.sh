#!/usr/bin/env bash

set -euo pipefail

PROFILE="${1:-}"
case "${PROFILE}" in
  tiger|10.4)
    PROFILE_NAME="tiger"
    DEFAULT_ARCHS="ppc;i386"
    BUILD_ROOT_NAME="macos-10.4-ub1"
    BUILD_SCRIPT_NAME="build-10_4.sh"
    ;;
  leopard|10.5)
    PROFILE_NAME="leopard"
    DEFAULT_ARCHS="ppc;i386;x86_64"
    BUILD_ROOT_NAME="macos-10.5-ub1"
    BUILD_SCRIPT_NAME="build-10_5.sh"
    ;;
  *)
    echo "Usage: $0 tiger|leopard" >&2
    exit 2
    ;;
esac

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROOT_DIR="${PROJECT_DIR}"
. "${SCRIPT_DIR}/macos/lib-common.sh"
. "${SCRIPT_DIR}/presentation-stage.sh"

if [[ -n "${TARGET:-}" ]]; then
  echo "The UB1 standalone release selects its complete target set; unset TARGET." >&2
  exit 2
fi
if [[ -n "${TARGET_SET:-}" && "${TARGET_SET}" != "standalone-release" ]]; then
  echo "The UB1 standalone release requires TARGET_SET=standalone-release." >&2
  exit 2
fi

export TARGET_SET=standalone-release
export ARCHS="${ARCHS:-${DEFAULT_ARCHS}}"
if [[ -z "${LOKA_LIPO_BIN:-}" ]]; then
  LOKA_LIPO_BIN="$(loka_find_selected_lipo || true)"
  if [[ -z "${LOKA_LIPO_BIN}" ]]; then
    echo "lipo was not found for the UB1 release." >&2
    exit 1
  fi
  export LOKA_LIPO_BIN
fi
if [[ "${PROFILE_NAME}" == "tiger" && "${ARCHS}" != "ppc;i386" ]]; then
  echo "The Tiger UB1 release requires ARCHS=ppc;i386 so its two slices can be merged." >&2
  exit 2
fi
if [[ "${PROFILE_NAME}" == "leopard" ]]; then
  case ";${ARCHS};" in
    *";ppc;"*|*";ppc7400;"*) ;;
    *)
      echo "The Leopard UB1 release requires a ppc or ppc7400 slice." >&2
      exit 2
      ;;
  esac
  case ";${ARCHS};" in
    *";i386;"*) ;;
    *)
      echo "The Leopard UB1 release requires an i386 slice." >&2
      exit 2
      ;;
  esac
fi

"${SCRIPT_DIR}/macos/${BUILD_SCRIPT_NAME}"

BUILD_ROOT="${PROJECT_DIR}/build/${BUILD_ROOT_NAME}"
UNIVERSAL_ROOT="${BUILD_ROOT}/universal"
STAGE_ROOT="${PROJECT_DIR}/build/release/macos-${PROFILE_NAME}-ub1"

copy_bundle() {
  local source_bundle="$1"
  local destination_bundle="$2"

  if [[ -x /usr/bin/ditto ]]; then
    /usr/bin/ditto "${source_bundle}" "${destination_bundle}"
  else
    cp -R "${source_bundle}" "${destination_bundle}"
  fi
}

populate_ub1_release() {
  local destination="$1"
  local target_name=""
  local record=""
  local _memberships=""
  local output_shape=""
  local rel_path=""
  local bundle_rel_path=""
  local source_item=""
  local destination_item=""
  local destination_binary=""
  local arch=""
  local release_archs=()
  local count=0

  for target_name in $(loka_targets_for_selection standalone-release); do
    record="$(loka_target_record "${target_name}")"
    IFS='|' read -r target_name _memberships output_shape rel_path <<< "${record}"
    case "${output_shape}" in
      executable)
        source_item="${UNIVERSAL_ROOT}/$(basename "${rel_path}")"
        destination_item="${destination}/$(basename "${rel_path}")"
        if [[ ! -f "${source_item}" ]]; then
          echo "UB1 release executable not found: ${source_item}" >&2
          return 1
        fi
        cp "${source_item}" "${destination_item}"
        destination_binary="${destination_item}"
        ;;
      bundle)
        bundle_rel_path="${rel_path%%/Contents/MacOS/*}"
        source_item="${UNIVERSAL_ROOT}/$(basename "${bundle_rel_path}")"
        destination_item="${destination}/$(basename "${bundle_rel_path}")"
        if [[ ! -d "${source_item}" ]]; then
          echo "UB1 release bundle not found: ${source_item}" >&2
          return 1
        fi
        copy_bundle "${source_item}" "${destination_item}"
        destination_binary="${destination_item}/Contents/MacOS/$(basename "${rel_path}")"
        ;;
      *)
        echo "Unknown UB1 release output shape '${output_shape}' for ${target_name}." >&2
        return 1
        ;;
    esac

    if [[ ! -f "${destination_binary}" ]]; then
      echo "Staged UB1 executable not found: ${destination_binary}" >&2
      return 1
    fi
    IFS=';' read -r -a release_archs <<< "${ARCHS}"
    for arch in "${release_archs[@]}"; do
      if ! loka_binary_contains_arch "${destination_binary}" "${arch}"; then
        echo "Staged UB1 executable does not contain ${arch}: ${destination_binary}" >&2
        return 1
      fi
    done
    count=$((count + 1))
  done

  if [[ "${count}" -ne 6 ]]; then
    echo "The UB1 standalone release must contain five loops plus SimpleViewer; found ${count}." >&2
    return 1
  fi
  printf '%s\n' \
    'Loka 0.0.4 UB1 Release applications' \
    '' \
    "Profile: ${PROFILE_NAME}" \
    "Architectures: ${ARCHS}" \
    '' \
    'The five StandaloneLoop applications run their UI tour repeatedly.' \
    'Quit a loop application to stop it. LokaSimpleViewerMacOS remains interactive.' \
    >"${destination}/README.txt"
}

loka_replace_stage_directory "${STAGE_ROOT}" populate_ub1_release
echo "Staged five autonomous UB1 loops plus SimpleViewer (${ARCHS}): ${STAGE_ROOT}"
