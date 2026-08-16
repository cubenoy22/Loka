#!/usr/bin/env bash
set -euo pipefail

export LC_ALL=C
export LANG=C

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LEGACY_DEVELOPER_DIR="${LOKA_LEGACY_DEVELOPER_DIR:-/Developer}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/ios-legacy-armv6}"
SDK_DIR="${LEGACY_DEVELOPER_DIR}/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS4.3.sdk"
PLATFORM_BIN="${LEGACY_DEVELOPER_DIR}/Platforms/iPhoneOS.platform/Developer/usr/bin"
COMPILER="${PLATFORM_BIN}/g++-4.2"
OTOOL="${LEGACY_DEVELOPER_DIR}/usr/bin/otool"
SOURCE="${ROOT_DIR}/apple/ios/legacy/UIKitHelloWorld.mm"
INFO_PLIST="${ROOT_DIR}/apple/ios/legacy/Info.plist"
OUTPUT_ZIP="${BUILD_DIR}/LokaLegacyHelloWorld-armv6-unsigned.zip"

require_file() {
  if [[ ! -f "$1" ]]; then
    echo "error: missing $2: $1" >&2
    exit 1
  fi
}

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: the ARMv6 golden build requires the preserved Apple toolchain on macOS." >&2
  exit 1
fi

require_file "${COMPILER}" "Xcode 3.2.6 iPhoneOS GCC 4.2"
require_file "${SDK_DIR}/System/Library/Frameworks/UIKit.framework/UIKit" "iPhoneOS 4.3 UIKit"
require_file "${SDK_DIR}/usr/lib/libSystem.dylib" "iPhoneOS 4.3 libSystem"
require_file "${OTOOL}" "Xcode otool"
require_file "${SOURCE}" "legacy UIKit source"
require_file "${INFO_PLIST}" "legacy Info.plist"

mkdir -p "${BUILD_DIR}"
STAGE_DIR="$(mktemp -d "${BUILD_DIR}/.loka-armv6.XXXXXX")"

cleanup() {
  /bin/rm -rf "${STAGE_DIR}"
}
trap cleanup EXIT HUP INT TERM

APP_DIR="${STAGE_DIR}/LokaLegacyHelloWorld.app"
EXECUTABLE="${APP_DIR}/LokaLegacyHelloWorld"
OBJECT="${STAGE_DIR}/UIKitHelloWorld.o"
STAGED_ZIP="${STAGE_DIR}/LokaLegacyHelloWorld-armv6-unsigned.zip"

mkdir -p "${APP_DIR}"

"${COMPILER}" \
  -arch armv6 \
  -isysroot "${SDK_DIR}" \
  -miphoneos-version-min=3.1 \
  -std=c++98 \
  -fno-exceptions \
  -fno-rtti \
  -Wall \
  -Wextra \
  -Werror \
  -c "${SOURCE}" \
  -o "${OBJECT}"

"${COMPILER}" \
  -arch armv6 \
  -isysroot "${SDK_DIR}" \
  -miphoneos-version-min=3.1 \
  -fno-exceptions \
  -fno-rtti \
  "${OBJECT}" \
  -framework UIKit \
  -framework Foundation \
  -framework CoreGraphics \
  -o "${EXECUTABLE}"

cp "${INFO_PLIST}" "${APP_DIR}/Info.plist"
chmod 0755 "${EXECUTABLE}"
/usr/bin/printf 'APPL????' > "${APP_DIR}/PkgInfo"
/usr/bin/plutil -lint "${APP_DIR}/Info.plist"

MACH_HEADER="$("${OTOOL}" -hv "${EXECUTABLE}")"
printf '%s\n' "${MACH_HEADER}"
if ! printf '%s\n' "${MACH_HEADER}" | /usr/bin/grep -Eq 'ARM[[:space:]]+V6'; then
  echo "error: linked executable is not ARMv6." >&2
  exit 1
fi

/usr/bin/ditto -c -k --keepParent "${APP_DIR}" "${STAGED_ZIP}"
mv "${STAGED_ZIP}" "${OUTPUT_ZIP}"

echo "Built unsigned iPhone OS 3.1 bundle:"
echo "  ${OUTPUT_ZIP}"
/usr/bin/shasum -a 256 "${OUTPUT_ZIP}"
