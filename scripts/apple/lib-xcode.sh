#!/usr/bin/env bash

loka_selected_developer_dir() {
  if [[ -n "${DEVELOPER_DIR:-}" ]]; then
    echo "${DEVELOPER_DIR}"
    return 0
  fi

  if command -v xcode-select >/dev/null 2>&1; then
    xcode-select -print-path 2>/dev/null || true
    return 0
  fi

  return 1
}

loka_require_full_xcode_for_generator() {
  local label="$1"
  local min_major="${2:-}"
  local selected_path=""
  local version_output=""
  local version=""
  local major=""

  if ! command -v xcodebuild >/dev/null 2>&1; then
    echo "error: xcodebuild was not found." >&2
    echo "Install a full Xcode.app and select it in Xcode > Settings > Locations > Command Line Tools." >&2
    echo "You can also run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
  fi

  if command -v xcode-select >/dev/null 2>&1; then
    selected_path="$(xcode-select -p 2>/dev/null || true)"
  fi

  version_output="$(xcodebuild -version 2>&1 || true)"
  version="$(printf "%s\n" "${version_output}" | awk '/^Xcode / {print $2; exit}' || true)"
  if [[ -z "${version}" ]]; then
    echo "error: unable to read an Xcode version from xcodebuild." >&2
    if [[ -n "${selected_path}" ]]; then
      echo "xcode-select currently points to: ${selected_path}" >&2
    fi
    echo "A full Xcode.app must be selected, not Command Line Tools alone." >&2
    echo "Open Xcode > Settings > Locations and choose your Xcode under Command Line Tools." >&2
    echo "You can also run: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    echo "xcodebuild output was:" >&2
    printf "%s\n" "${version_output}" >&2
    exit 1
  fi

  if [[ -n "${min_major}" ]]; then
    major="${version%%.*}"
    if [[ "${major}" =~ ^[0-9]+$ && "${major}" -lt "${min_major}" ]]; then
      echo "error: CMake's Xcode generator requires Xcode ${min_major}.0 or newer for this script." >&2
      if [[ -n "${selected_path}" ]]; then
        echo "xcode-select currently points to: ${selected_path}" >&2
      fi
      echo "Generate on a newer Mac, then copy the project to the older Xcode host for testing." >&2
      exit 1
    fi
  fi

  echo "[${label}] xcodebuild version=${version}"
  if [[ -n "${selected_path}" ]]; then
    echo "[${label}] xcode-select path=${selected_path}"
  fi
}

loka_require_cmake_xcode_generator() {
  if ! command -v cmake >/dev/null 2>&1; then
    echo "error: cmake was not found." >&2
    echo "Install CMake with Xcode generator support before running this script." >&2
    exit 1
  fi

  if ! cmake --help 2>/dev/null | grep -q "Xcode"; then
    echo "error: this CMake build does not provide the Xcode generator." >&2
    echo "Install a CMake build with Xcode generator support." >&2
    exit 1
  fi
}
