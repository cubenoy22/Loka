#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ROOT_DIR="${REPO_DIR}"
source "${REPO_DIR}/scripts/macos/lib-common.sh"

fail() {
  echo "MacOSBuildTargetsTest: $*" >&2
  exit 1
}

EXPECTED_TARGETS=$'LokaFloppyBirdMacOS\nLokaHelloMacOS\nLokaMineMacOS\nLokaSimpleViewerMacOS\nScrapbookUIMacOS\nLokaTutorialMacOS'
ACTUAL_TARGETS="$(loka_known_targets)"
[[ "${ACTUAL_TARGETS}" == "${EXPECTED_TARGETS}" ]] ||
  fail "default target manifest does not contain the six shipping examples"

SCRAPBOOK_REL_PATH="example/ScrapbookUI/ScrapbookUIMacOS.app/Contents/MacOS/ScrapbookUIMacOS"
[[ "$(loka_target_rel_path ScrapbookUIMacOS)" == "${SCRAPBOOK_REL_PATH}" ]] ||
  fail "ScrapbookUIMacOS executable path does not point inside its bundle"
[[ "$(loka_target_rel_path LokaScrapbookStandaloneFlowMacOS)" == \
  "apple/macos/LokaScrapbookStandaloneFlowMacOS.app/Contents/MacOS/LokaScrapbookStandaloneFlowMacOS" ]] ||
  fail "explicit-only standalone Flow bundle path changed"

TEST_ROOT="$(mktemp -d /tmp/loka-macos-build-targets-XXXXXX)"
trap 'rm -rf "${TEST_ROOT}"' EXIT

FAKE_ROOT="${TEST_ROOT}/fake-root"
mkdir -p "${FAKE_ROOT}/scripts/macos"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'printf "%s\n" "${TARGET:-}" >> "${LOKA_TEST_BUILD_LOG}"' \
  > "${FAKE_ROOT}/scripts/macos/build.sh"
chmod +x "${FAKE_ROOT}/scripts/macos/build.sh"

export LOKA_TEST_BUILD_LOG="${TEST_ROOT}/targets.log"
unset TARGET || true
loka_build_requested_or_known_targets "${FAKE_ROOT}"
[[ "$(cat "${LOKA_TEST_BUILD_LOG}")" == "${EXPECTED_TARGETS}" ]] ||
  fail "default build loop did not request all six shipping examples"

STALE_ROOT="${TEST_ROOT}/stale"
mkdir -p \
  "${STALE_ROOT}/example/HelloWorld/LokaHelloMacOS" \
  "${STALE_ROOT}/example/ScrapbookUI/ScrapbookUIMacOS.app/Contents/Resources" \
  "${STALE_ROOT}/apple/macos/LokaScrapbookStandaloneFlowMacOS.app/Contents/MacOS"
loka_cleanup_stale_output_dirs "${STALE_ROOT}"
[[ ! -e "${STALE_ROOT}/example/HelloWorld/LokaHelloMacOS" ]] ||
  fail "plain-executable stale output directory was not removed"
[[ ! -e "${STALE_ROOT}/example/ScrapbookUI/ScrapbookUIMacOS.app" ]] ||
  fail "ScrapbookUI stale bundle was not removed at its bundle root"
[[ ! -e "${STALE_ROOT}/apple/macos/LokaScrapbookStandaloneFlowMacOS.app" ]] ||
  fail "explicit-only stale bundle was not removed at its bundle root"

FAKE_BIN="${TEST_ROOT}/bin"
mkdir -p "${FAKE_BIN}"
printf '%s\n' \
  '#!/usr/bin/env bash' \
  'if [[ "${LOKA_TEST_LIPO_FAIL:-0}" == "1" ]]; then exit 73; fi' \
  'if [[ "$1" == "-info" ]]; then exit 0; fi' \
  'output=""' \
  'while [[ "$#" -gt 0 ]]; do' \
  '  if [[ "$1" == "-output" ]]; then output="$2"; shift 2; else shift; fi' \
  'done' \
  'printf "universal\n" > "${output}"' \
  > "${FAKE_BIN}/lipo"
chmod +x "${FAKE_BIN}/lipo"
export PATH="${FAKE_BIN}:${PATH}"

stage_arch() {
  local build_root="$1"
  local arch="$2"
  local target_name
  local selection
  local output_shape
  local rel_path
  local target_path
  local bundle_root

  while IFS='|' read -r target_name selection output_shape rel_path; do
    [[ "${selection}" == "default" ]] || continue
    target_path="${build_root}/Release-${arch}/${rel_path}"
    mkdir -p "$(dirname "${target_path}")"
    printf '%s\n' "${arch}" > "${target_path}"
    if [[ "${output_shape}" == "bundle" ]]; then
      bundle_root="${target_path%%/Contents/MacOS/*}"
      mkdir -p "${bundle_root}/Contents/Resources"
      printf '%s\n' "assets-${arch}" > "${bundle_root}/Contents/Resources/ASSETS.LRP"
    fi
  done < <(loka_target_manifest)
}

REGRESSION_FAILURES=()

ATOMIC_ROOT="${TEST_ROOT}/atomic"
stage_arch "${ATOMIC_ROOT}" ppc
stage_arch "${ATOMIC_ROOT}" i386
ATOMIC_BUNDLE="${ATOMIC_ROOT}/universal/ScrapbookUIMacOS.app"
mkdir -p "${ATOMIC_BUNDLE}/Contents/MacOS" "${ATOMIC_BUNDLE}/Contents/Resources"
printf '%s\n' "previous-universal" > "${ATOMIC_BUNDLE}/Contents/MacOS/ScrapbookUIMacOS"
printf '%s\n' "previous-assets" > "${ATOMIC_BUNDLE}/Contents/Resources/ASSETS.LRP"
set +e
TARGET=ScrapbookUIMacOS LOKA_TEST_LIPO_FAIL=1 \
  loka_merge_requested_or_known_targets_two_arch \
  "${ATOMIC_ROOT}" Release ppc i386
ATOMIC_STATUS=$?
set -e
[[ "${ATOMIC_STATUS}" -ne 0 ]] ||
  REGRESSION_FAILURES+=("a failing lipo was reported as success")
[[ "$(cat "${ATOMIC_BUNDLE}/Contents/MacOS/ScrapbookUIMacOS")" == "previous-universal" ]] ||
  REGRESSION_FAILURES+=("a failing lipo replaced the previous bundle executable")
[[ "$(cat "${ATOMIC_BUNDLE}/Contents/Resources/ASSETS.LRP")" == "previous-assets" ]] ||
  REGRESSION_FAILURES+=("a failing lipo replaced the previous bundle resources")

MISSING_ROOT="${TEST_ROOT}/missing-arch"
stage_arch "${MISSING_ROOT}" ppc
mkdir -p "${MISSING_ROOT}/universal"
printf '%s\n' "previous-universal" > "${MISSING_ROOT}/universal/LokaHelloMacOS"
set +e
MISSING_OUTPUT="$(TARGET=LokaHelloMacOS \
  loka_merge_requested_or_known_targets_two_arch \
  "${MISSING_ROOT}" Release ppc i386 2>&1)"
MISSING_STATUS=$?
set -e
[[ "${MISSING_STATUS}" -ne 0 ]] ||
  REGRESSION_FAILURES+=("a merge with no i386 input was reported as success")
[[ "${MISSING_OUTPUT}" == *"i386"* ]] ||
  REGRESSION_FAILURES+=("the missing-input error did not name i386")
[[ "$(cat "${MISSING_ROOT}/universal/LokaHelloMacOS")" == "previous-universal" ]] ||
  REGRESSION_FAILURES+=("a missing i386 input published the thin ppc executable")

if [[ "${#REGRESSION_FAILURES[@]}" -ne 0 ]]; then
  printf 'MacOSBuildTargetsTest: %s\n' "${REGRESSION_FAILURES[@]}" >&2
  exit 1
fi

TWO_ARCH_ROOT="${TEST_ROOT}/two-arch"
stage_arch "${TWO_ARCH_ROOT}" ppc
stage_arch "${TWO_ARCH_ROOT}" i386
unset TARGET || true
loka_merge_requested_or_known_targets_two_arch \
  "${TWO_ARCH_ROOT}" Release ppc i386
[[ "$(cat "${TWO_ARCH_ROOT}/universal/LokaHelloMacOS")" == "universal" ]] ||
  fail "two-arch merge regressed a plain executable"
[[ "$(cat "${TWO_ARCH_ROOT}/universal/ScrapbookUIMacOS.app/Contents/MacOS/ScrapbookUIMacOS")" == "universal" ]] ||
  fail "two-arch merge did not place ScrapbookUI's executable inside a bundle"
[[ "$(cat "${TWO_ARCH_ROOT}/universal/ScrapbookUIMacOS.app/Contents/Resources/ASSETS.LRP")" == "assets-ppc" ]] ||
  fail "two-arch merge did not preserve ScrapbookUI's Resources"

MULTI_ARCH_ROOT="${TEST_ROOT}/multi-arch"
stage_arch "${MULTI_ARCH_ROOT}" ppc
stage_arch "${MULTI_ARCH_ROOT}" i386
stage_arch "${MULTI_ARCH_ROOT}" x86_64
unset TARGET || true
loka_merge_requested_or_known_targets_multi_arch \
  "${MULTI_ARCH_ROOT}" Release 'ppc;i386;x86_64'
[[ "$(cat "${MULTI_ARCH_ROOT}/universal/LokaHelloMacOS")" == "universal" ]] ||
  fail "multi-arch merge regressed a plain executable"
[[ "$(cat "${MULTI_ARCH_ROOT}/universal/ScrapbookUIMacOS.app/Contents/MacOS/ScrapbookUIMacOS")" == "universal" ]] ||
  fail "multi-arch merge did not place ScrapbookUI's executable inside a bundle"
[[ "$(cat "${MULTI_ARCH_ROOT}/universal/ScrapbookUIMacOS.app/Contents/Resources/ASSETS.LRP")" == "assets-ppc" ]] ||
  fail "multi-arch merge did not preserve ScrapbookUI's Resources"

echo "MacOSBuildTargetsTest: PASS"
