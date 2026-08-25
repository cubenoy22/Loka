#!/usr/bin/env bash

# Host-local Retro68 policy layered on the shared dotenv mechanism.

LOKA_RETRO68_ENV_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$LOKA_RETRO68_ENV_SCRIPT_DIR/env-file.sh"
unset LOKA_RETRO68_ENV_SCRIPT_DIR

loka_load_retro68_environment() {
  local project_dir="$1"
  local path="${RETRO68_ENV_FILE:-$project_dir/.env-retro68}"

  if [ -f "$path" ]; then
    loka_import_environment_file "$path" \
      "RETRO68_BUILD_DIR RETRO68_TOOLCHAIN_DIR RETRO68_TOOLCHAIN_BIN CMAKE_MAKE_PROGRAM" \
      1
  fi
}
