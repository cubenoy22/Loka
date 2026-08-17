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
  local plain_path="${build_root}/Release-${arch}/example/HelloWorld/LokaHelloMacOS"
  local bundle_root="${build_root}/Release-${arch}/example/ScrapbookUI/ScrapbookUIMacOS.app"
  mkdir -p "$(dirname "${plain_path}")" \
    "${bundle_root}/Contents/MacOS" "${bundle_root}/Contents/Resources"
  printf '%s\n' "${arch}" > "${plain_path}"
  printf '%s\n' "${arch}" > "${bundle_root}/Contents/MacOS/ScrapbookUIMacOS"
  printf '%s\n' "assets-${arch}" > "${bundle_root}/Contents/Resources/ASSETS.LRP"
}

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
