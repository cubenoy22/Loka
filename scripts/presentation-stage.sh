#!/usr/bin/env bash

# Replaces a completed presentation directory only after its caller has
# populated and validated a temporary sibling. Run this before installing a
# caller-owned EXIT trap; the subshell keeps transaction cleanup local.
loka_replace_stage_directory() (
  set -euo pipefail

  if [[ $# -ne 2 ]]; then
    echo "loka_replace_stage_directory requires a stage path and populate function." >&2
    return 2
  fi

  local stage_root="$1"
  local populate_function="$2"
  local parent=""
  local stage_name=""
  local staging_root=""
  local backup_root=""

  case "$stage_root" in
    ""|/|.|..)
      echo "Refusing unsafe presentation stage path: $stage_root" >&2
      return 2
      ;;
  esac
  parent="$(dirname "$stage_root")"
  stage_name="$(basename "$stage_root")"
  if [[ "$parent" == "/" || "$stage_name" == "." || "$stage_name" == ".." ]]; then
    echo "Refusing unsafe presentation stage path: $stage_root" >&2
    return 2
  fi

  mkdir -p "$parent"
  staging_root="$parent/.${stage_name}.staging.$$"
  backup_root="$parent/.${stage_name}.previous.$$"

  restore_stage_on_exit() {
    local exit_code=$?
    if [[ -e "$backup_root" ]]; then
      if [[ -e "$stage_root" ]]; then
        rm -rf "$backup_root"
      else
        mv "$backup_root" "$stage_root"
      fi
    fi
    if [[ -e "$staging_root" ]]; then
      rm -rf "$staging_root"
    fi
    return "$exit_code"
  }
  trap restore_stage_on_exit EXIT

  rm -rf "$staging_root" "$backup_root"
  mkdir -p "$staging_root"
  "$populate_function" "$staging_root"
  if [[ ! -d "$staging_root" ]]; then
    echo "Presentation populate function removed its staging directory." >&2
    return 1
  fi

  if [[ -e "$stage_root" ]]; then
    mv "$stage_root" "$backup_root"
  fi
  mv "$staging_root" "$stage_root"
  rm -rf "$backup_root"
)

# Answers whether a Mach-O binary contains the given architecture. `lipo
# -archs` only exists in the Xcode 10+ cctools, so this reads `lipo -info`,
# the surface every supported host shares; the architecture list follows the
# last ": " for both the thin ("is architecture:") and fat ("are:") wordings,
# including binary paths that themselves contain ": ". LOKA_LIPO_BIN lets
# tests substitute a scripted lipo.
loka_binary_contains_arch() {
  local binary="$1"
  local arch="$2"
  "${LOKA_LIPO_BIN:-/usr/bin/lipo}" -info "$binary" | sed 's/^.*: //' | tr ' ' '\n' | grep -Fxq "$arch"
}
