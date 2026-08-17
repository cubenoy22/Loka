#!/usr/bin/env bash
set -euo pipefail

source "${ROOT_DIR}/scripts/apple/lib-xcode.sh"

loka_target_manifest() {
  # target | default selection | output shape | executable path
  printf '%s\n' \
    "LokaFloppyBirdMacOS|default|executable|example/FloppyBird/LokaFloppyBirdMacOS" \
    "LokaHelloMacOS|default|executable|example/HelloWorld/LokaHelloMacOS" \
    "LokaMineMacOS|default|executable|example/MineSweeper/LokaMineMacOS" \
    "LokaSimpleViewerMacOS|default|executable|example/SimpleViewer/LokaSimpleViewerMacOS" \
    "ScrapbookUIMacOS|default|bundle|example/ScrapbookUI/ScrapbookUIMacOS.app/Contents/MacOS/ScrapbookUIMacOS" \
    "LokaTutorialMacOS|default|executable|example/Tutorial/LokaTutorialMacOS" \
    "LokaScrapbookStandaloneFlowMacOS|explicit|bundle|apple/macos/LokaScrapbookStandaloneFlowMacOS.app/Contents/MacOS/LokaScrapbookStandaloneFlowMacOS"
}

loka_known_targets() {
  local target_name
  local selection
  local _output_shape
  local _rel_path

  while IFS='|' read -r target_name selection _output_shape _rel_path; do
    if [[ "${selection}" == "default" ]]; then
      echo "${target_name}"
    fi
  done < <(loka_target_manifest)
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

loka_target_record() {
  local requested_target="$1"
  local target_name
  local selection
  local output_shape
  local rel_path

  while IFS='|' read -r target_name selection output_shape rel_path; do
    if [[ "${target_name}" == "${requested_target}" ]]; then
      printf '%s|%s|%s|%s\n' \
        "${target_name}" "${selection}" "${output_shape}" "${rel_path}"
      return 0
    fi
  done < <(loka_target_manifest)

  return 1
}

loka_target_rel_path() {
  local requested_target="$1"
  local record

  record="$(loka_target_record "${requested_target}" || true)"
  if [[ -z "${record}" ]]; then
    return 1
  fi
  echo "${record##*|}"
}

loka_cleanup_stale_output_dirs() {
  local build_dir="$1"
  local target_name
  local _selection
  local output_shape
  local rel_path
  local cleanup_rel_path

  while IFS='|' read -r target_name _selection output_shape rel_path; do
    case "${output_shape}" in
      executable) cleanup_rel_path="${rel_path}" ;;
      bundle) cleanup_rel_path="${rel_path%%/Contents/MacOS/*}" ;;
      *)
        echo "error: unknown output shape '${output_shape}' for ${target_name}." >&2
        return 1
        ;;
    esac

    if [[ -d "${build_dir}/${cleanup_rel_path}" ]]; then
      rm -rf "${build_dir:?}/${cleanup_rel_path}"
    fi
  done < <(loka_target_manifest)
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

  mkdir -p "${build_root}/universal"

  if [[ -n "${TARGET:-}" ]]; then
    loka_merge_target_archs \
      "${build_root}" "${build_cfg}" "${arch_a};${arch_b}" "${TARGET}"
    return
  fi

  for target_name in $(loka_known_targets); do
    loka_merge_target_archs \
      "${build_root}" "${build_cfg}" "${arch_a};${arch_b}" "${target_name}"
  done
}

loka_merge_target_archs() {
  local build_root="$1"
  local build_cfg="$2"
  local archs_csv="$3"
  local requested_target="$4"
  local record
  local target_name
  local _selection
  local output_shape
  local rel_path
  local bundle_rel_path
  local source_bundle
  local out_bundle
  local out_bin
  local staging_root=""
  local staging_bundle
  local arch
  local bin
  local merge_inputs=()
  local IFS=';'

  record="$(loka_target_record "${requested_target}" || true)"
  if [[ -z "${record}" ]]; then
    return
  fi
  IFS='|' read -r target_name _selection output_shape rel_path <<< "${record}"
  IFS=';'

  for arch in ${archs_csv}; do
    bin="${build_root}/${build_cfg}-${arch}/${rel_path}"
    if [[ ! -f "${bin}" ]]; then
      echo "error: missing required ${arch} input for ${target_name}: ${bin}" >&2
      return 1
    fi
    merge_inputs+=("${bin}")
  done

  if [[ "${#merge_inputs[@]}" -eq 0 ]]; then
    echo "error: no architectures requested for ${target_name}." >&2
    return 1
  fi

  case "${output_shape}" in
    executable)
      out_bin="${build_root}/universal/$(basename "${rel_path}")"
      ;;
    bundle)
      bundle_rel_path="${rel_path%%/Contents/MacOS/*}"
      source_bundle=""
      for arch in ${archs_csv}; do
        if [[ -d "${build_root}/${build_cfg}-${arch}/${bundle_rel_path}" ]]; then
          source_bundle="${build_root}/${build_cfg}-${arch}/${bundle_rel_path}"
          break
        fi
      done
      if [[ -z "${source_bundle}" ]]; then
        echo "error: no source bundle found for ${target_name}." >&2
        return 1
      fi
      out_bundle="${build_root}/universal/$(basename "${bundle_rel_path}")"
      if ! staging_root="$(mktemp -d \
        "${build_root}/universal/.${target_name}.XXXXXX")"; then
        echo "error: could not create a staging directory for ${target_name}." >&2
        return 1
      fi
      staging_bundle="${staging_root}/$(basename "${bundle_rel_path}")"
      if ! cp -R "${source_bundle}" "${staging_bundle}"; then
        rm -rf "${staging_root}"
        return 1
      fi
      out_bin="${staging_bundle}/Contents/MacOS/$(basename "${rel_path}")"
      ;;
    *)
      echo "error: unknown output shape '${output_shape}' for ${target_name}." >&2
      return 1
      ;;
  esac

  if [[ "${#merge_inputs[@]}" -ge 2 ]]; then
    if ! lipo -create "${merge_inputs[@]}" -output "${out_bin}"; then
      [[ -z "${staging_root}" ]] || rm -rf "${staging_root}"
      return 1
    fi
    if ! lipo -info "${out_bin}"; then
      [[ -z "${staging_root}" ]] || rm -rf "${staging_root}"
      return 1
    fi
  else
    if ! cp -f "${merge_inputs[0]}" "${out_bin}"; then
      [[ -z "${staging_root}" ]] || rm -rf "${staging_root}"
      return 1
    fi
  fi

  if [[ -n "${staging_root}" ]]; then
    if ! rm -rf "${out_bundle}"; then
      rm -rf "${staging_root}"
      return 1
    fi
    if ! mv "${staging_bundle}" "${out_bundle}"; then
      rm -rf "${staging_root}"
      return 1
    fi
    rm -rf "${staging_root}"
  fi
}

loka_merge_requested_or_known_targets_multi_arch() {
  local build_root="$1"
  local build_cfg="$2"
  local archs_csv="$3"
  local target_name

  mkdir -p "${build_root}/universal"

  if [[ -n "${TARGET:-}" ]]; then
    loka_merge_target_archs \
      "${build_root}" "${build_cfg}" "${archs_csv}" "${TARGET}"
    return
  fi

  for target_name in $(loka_known_targets); do
    loka_merge_target_archs \
      "${build_root}" "${build_cfg}" "${archs_csv}" "${target_name}"
  done
}
