#!/usr/bin/env bash
set -euo pipefail

source "${ROOT_DIR}/scripts/apple/lib-xcode.sh"

loka_known_targets() {
  echo "LokaFloppyBirdMacOS"
  echo "LokaHelloMacOS"
  echo "LokaMineMacOS"
  echo "LokaSimpleViewerMacOS"
  echo "LokaTutorialMacOS"
}

loka_find_selected_tool() {
  local tool="$1"
  local found=""

  if command -v xcrun >/dev/null 2>&1; then
    found="$(xcrun -find "${tool}" 2>/dev/null || true)"
    if [[ -n "${found}" && -x "${found}" ]]; then
      echo "${found}"
      return 0
    fi
  fi

  if command -v "${tool}" >/dev/null 2>&1; then
    command -v "${tool}"
    return 0
  fi

  return 1
}

loka_find_first_selected_tool() {
  local tool=""
  local found=""

  for tool in "$@"; do
    found="$(loka_find_selected_tool "${tool}" || true)"
    if [[ -n "${found}" ]]; then
      echo "${found}"
      return 0
    fi
  done

  return 1
}

loka_find_selected_sdk() {
  local sdk_name="$1"
  local developer_dir=""
  local candidate=""
  local candidates=()

  developer_dir="$(loka_selected_developer_dir || true)"
  if [[ -n "${developer_dir}" ]]; then
    candidates+=("${developer_dir}/SDKs/${sdk_name}")
    candidates+=("${developer_dir}/Platforms/MacOSX.platform/Developer/SDKs/${sdk_name}")
  fi

  candidates+=("/Developer/SDKs/${sdk_name}")
  candidates+=("/Applications/Xcode.app/Contents/Developer/SDKs/${sdk_name}")
  candidates+=("/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/${sdk_name}")

  for candidate in "${candidates[@]}"; do
    if [[ -d "${candidate}" ]]; then
      echo "${candidate}"
      return 0
    fi
  done

  return 1
}

loka_target_rel_path() {
  case "$1" in
    LokaFloppyBirdMacOS) echo "example/FloppyBird/LokaFloppyBirdMacOS" ;;
    LokaHelloMacOS) echo "example/HelloWorld/LokaHelloMacOS" ;;
    LokaMineMacOS) echo "example/MineSweeper/LokaMineMacOS" ;;
    LokaSimpleViewerMacOS) echo "example/SimpleViewer/LokaSimpleViewerMacOS" ;;
    LokaScrapbookStandaloneFlowMacOS)
      echo "apple/macos/LokaScrapbookStandaloneFlowMacOS.app/Contents/MacOS/LokaScrapbookStandaloneFlowMacOS"
      ;;
    LokaTutorialMacOS) echo "example/Tutorial/LokaTutorialMacOS" ;;
    *) return 1 ;;
  esac
}

loka_cleanup_stale_output_dirs() {
  local build_dir="$1"
  local rel_path
  for rel_path in \
    "example/FloppyBird/LokaFloppyBirdMacOS" \
    "example/HelloWorld/LokaHelloMacOS" \
    "example/MineSweeper/LokaMineMacOS" \
    "example/SimpleViewer/LokaSimpleViewerMacOS" \
    "example/Tutorial/LokaTutorialMacOS"; do
    if [[ -d "${build_dir}/${rel_path}" ]]; then
      rm -rf "${build_dir:?}/${rel_path}"
    fi
  done
}

loka_build_requested_or_known_targets() {
  local root_dir="$1"
  local build_script="${root_dir}/scripts/macos/build.sh"
  local t
  if [[ -n "${TARGET:-}" ]]; then
    "${build_script}"
    return
  fi
  for t in $(loka_known_targets); do
    export TARGET="${t}"
    "${build_script}"
  done
  unset TARGET || true
}

loka_merge_requested_or_known_targets_two_arch() {
  local build_root="$1"
  local build_cfg="$2"
  local arch_a="$3"
  local arch_b="$4"
  local target_name
  local rel_path
  local bin_a
  local bin_b
  local out_bin

  mkdir -p "${build_root}/universal"

  if [[ -n "${TARGET:-}" ]]; then
    rel_path="$(loka_target_rel_path "${TARGET}" || true)"
    if [[ -n "${rel_path}" ]]; then
      bin_a="${build_root}/${build_cfg}-${arch_a}/${rel_path}"
      bin_b="${build_root}/${build_cfg}-${arch_b}/${rel_path}"
      out_bin="${build_root}/universal/$(basename "${rel_path}")"
      if [[ -f "${bin_a}" && -f "${bin_b}" ]]; then
        lipo -create "${bin_a}" "${bin_b}" -output "${out_bin}"
        lipo -info "${out_bin}"
      fi
    fi
    return
  fi

  for target_name in $(loka_known_targets); do
    rel_path="$(loka_target_rel_path "${target_name}")"
    bin_a="${build_root}/${build_cfg}-${arch_a}/${rel_path}"
    bin_b="${build_root}/${build_cfg}-${arch_b}/${rel_path}"
    out_bin="${build_root}/universal/$(basename "${rel_path}")"
    if [[ -f "${bin_a}" && -f "${bin_b}" ]]; then
      lipo -create "${bin_a}" "${bin_b}" -output "${out_bin}"
      lipo -info "${out_bin}"
    fi
  done
}

loka_merge_requested_or_known_targets_multi_arch() {
  local build_root="$1"
  local build_cfg="$2"
  local archs_csv="$3"
  local target_name
  local rel_path
  local out_bin
  local arch
  local bin
  local inputs
  local IFS

  mkdir -p "${build_root}/universal"

  merge_one() {
    local merge_rel_path="$1"
    local merge_out_bin="${build_root}/universal/$(basename "${merge_rel_path}")"
    local merge_inputs=()
    local IFS=';'
    for arch in ${archs_csv}; do
      bin="${build_root}/${build_cfg}-${arch}/${merge_rel_path}"
      if [[ -f "${bin}" ]]; then
        merge_inputs+=("${bin}")
      fi
    done
    if [[ "${#merge_inputs[@]}" -ge 2 ]]; then
      lipo -create "${merge_inputs[@]}" -output "${merge_out_bin}"
      lipo -info "${merge_out_bin}"
    elif [[ "${#merge_inputs[@]}" -eq 1 ]]; then
      cp -f "${merge_inputs[0]}" "${merge_out_bin}"
    fi
  }

  if [[ -n "${TARGET:-}" ]]; then
    rel_path="$(loka_target_rel_path "${TARGET}" || true)"
    if [[ -n "${rel_path}" ]]; then
      merge_one "${rel_path}"
    fi
    return
  fi

  for target_name in $(loka_known_targets); do
    rel_path="$(loka_target_rel_path "${target_name}")"
    merge_one "${rel_path}"
  done
}
