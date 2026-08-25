#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
. "$SCRIPT_DIR/retro68-env.sh"

loka_load_retro68_environment "$PROJECT_DIR"

find_cmake() {
  local candidate=""
  if command -v cmake >/dev/null 2>&1; then
    command -v cmake
    return 0
  fi
  for candidate in \
    /opt/homebrew/bin/cmake \
    /usr/local/bin/cmake \
    /opt/local/bin/cmake; do
    if [ -x "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

find_ninja() {
  local candidate=""
  if [ -n "${CMAKE_MAKE_PROGRAM:-}" ]; then
    if [ -x "$CMAKE_MAKE_PROGRAM" ]; then
      printf '%s\n' "$CMAKE_MAKE_PROGRAM"
      return 0
    fi
    if command -v "$CMAKE_MAKE_PROGRAM" >/dev/null 2>&1; then
      command -v "$CMAKE_MAKE_PROGRAM"
      return 0
    fi
    echo "Configured CMAKE_MAKE_PROGRAM was not found: $CMAKE_MAKE_PROGRAM" >&2
    return 1
  fi
  if command -v ninja >/dev/null 2>&1; then
    command -v ninja
    return 0
  fi
  for candidate in \
    /opt/homebrew/bin/ninja \
    /usr/local/bin/ninja \
    /opt/local/bin/ninja \
    /usr/bin/ninja; do
    if [ -x "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  return 1
}

CMAKE_BIN="$(find_cmake || true)"
if [ -z "$CMAKE_BIN" ]; then
  echo "CMake was not found. Install it or add it to PATH." >&2
  exit 1
fi

IS_CONFIGURE=1
case "${1:-}" in
  --build|--install|--open|-E|-P|--find-package|--help|--version|--system-information)
    IS_CONFIGURE=0
    ;;
esac

if [ "$IS_CONFIGURE" -eq 1 ]; then
  HAS_MAKE_PROGRAM=0
  HAS_BUILD_DIR=0
  HAS_TOOLCHAIN_DIR=0
  for argument in "$@"; do
    case "$argument" in
      -DCMAKE_MAKE_PROGRAM=*|-DCMAKE_MAKE_PROGRAM:*=*) HAS_MAKE_PROGRAM=1 ;;
      -DRETRO68_BUILD_DIR=*|-DRETRO68_BUILD_DIR:*=*) HAS_BUILD_DIR=1 ;;
      -DRETRO68_TOOLCHAIN_DIR=*|-DRETRO68_TOOLCHAIN_DIR:*=*) HAS_TOOLCHAIN_DIR=1 ;;
    esac
  done

  CONFIGURE_CACHE_ARGS=()
  if [ "$HAS_BUILD_DIR" -eq 0 ] && [ -n "${RETRO68_BUILD_DIR:-}" ]; then
    CONFIGURE_CACHE_ARGS+=("-DRETRO68_BUILD_DIR=$RETRO68_BUILD_DIR")
  fi
  if [ "$HAS_TOOLCHAIN_DIR" -eq 0 ] \
      && [ -n "${RETRO68_TOOLCHAIN_DIR:-}" ]; then
    CONFIGURE_CACHE_ARGS+=("-DRETRO68_TOOLCHAIN_DIR=$RETRO68_TOOLCHAIN_DIR")
  fi
  if [ "$HAS_MAKE_PROGRAM" -eq 0 ]; then
    NINJA_BIN="$(find_ninja || true)"
    if [ -z "$NINJA_BIN" ]; then
      echo "Ninja was not found. Install it or set CMAKE_MAKE_PROGRAM in .env-retro68." >&2
      exit 1
    fi
    CONFIGURE_CACHE_ARGS+=("-DCMAKE_MAKE_PROGRAM=$NINJA_BIN")
  fi
  exec "$CMAKE_BIN" "$@" "${CONFIGURE_CACHE_ARGS[@]}"
fi

exec "$CMAKE_BIN" "$@"
