#!/usr/bin/env bash

# Stages the target manifest's complete standalone-release selection from an
# already-built tree. The caller supplies the source layout because legacy
# split builds flatten their lipo outputs while modern CMake builds retain the
# target-relative paths.
loka_populate_standalone_release_stage() {
  local destination="$1"
  local source_root="$2"
  local source_layout="$3"
  local staged_archs="$4"
  local format_name="$5"
  local profile_name="$6"
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

  case "${source_layout}" in
    flat|tree) ;;
    *)
      echo "Unknown standalone release source layout '${source_layout}'." >&2
      return 2
      ;;
  esac

  for target_name in $(loka_targets_for_selection standalone-release); do
    record="$(loka_target_record "${target_name}")"
    IFS='|' read -r target_name _memberships output_shape rel_path <<< "${record}"
    case "${output_shape}" in
      executable)
        if [[ "${source_layout}" == "flat" ]]; then
          source_item="${source_root}/$(basename "${rel_path}")"
        else
          source_item="${source_root}/${rel_path}"
        fi
        destination_item="${destination}/$(basename "${rel_path}")"
        if [[ ! -f "${source_item}" ]]; then
          echo "${format_name} release executable not found: ${source_item}" >&2
          return 1
        fi
        cp "${source_item}" "${destination_item}"
        destination_binary="${destination_item}"
        ;;
      bundle)
        bundle_rel_path="${rel_path%%/Contents/MacOS/*}"
        if [[ "${source_layout}" == "flat" ]]; then
          source_item="${source_root}/$(basename "${bundle_rel_path}")"
        else
          source_item="${source_root}/${bundle_rel_path}"
        fi
        destination_item="${destination}/$(basename "${bundle_rel_path}")"
        if [[ ! -d "${source_item}" ]]; then
          echo "${format_name} release bundle not found: ${source_item}" >&2
          return 1
        fi
        if [[ -x /usr/bin/ditto ]]; then
          /usr/bin/ditto "${source_item}" "${destination_item}"
        else
          cp -R "${source_item}" "${destination_item}"
        fi
        destination_binary="${destination_item}/Contents/MacOS/$(basename "${rel_path}")"
        ;;
      *)
        echo "Unknown standalone release output shape '${output_shape}' for ${target_name}." >&2
        return 2
        ;;
    esac

    if [[ ! -f "${destination_binary}" ]]; then
      echo "Staged ${format_name} executable not found: ${destination_binary}" >&2
      return 1
    fi
    IFS=';' read -r -a release_archs <<< "${staged_archs}"
    for arch in "${release_archs[@]}"; do
      if ! loka_binary_contains_arch "${destination_binary}" "${arch}"; then
        echo "Staged ${format_name} executable does not contain ${arch}: ${destination_binary}" >&2
        return 1
      fi
    done
    count=$((count + 1))
  done

  if [[ "${count}" -ne 6 ]]; then
    echo "The ${format_name} standalone release must contain five loops plus SimpleViewer; found ${count}." >&2
    return 1
  fi
  printf '%s\n' \
    "Loka 0.0.4 ${format_name} Release applications" \
    '' \
    "Profile: ${profile_name}" \
    "Architectures: ${staged_archs}" \
    '' \
    'The five StandaloneLoop applications run their UI tour repeatedly.' \
    'Quit a loop application to stop it. LokaSimpleViewerMacOS remains interactive.' \
    >"${destination}/README.txt"
}

loka_stage_standalone_release() {
  if [[ $# -ne 6 ]]; then
    echo "loka_stage_standalone_release requires source, destination, layout, architectures, format, and profile." >&2
    return 2
  fi
  loka_replace_stage_directory \
    "$2" \
    loka_populate_standalone_release_stage \
    "$1" "$3" "$4" "$5" "$6"
}
