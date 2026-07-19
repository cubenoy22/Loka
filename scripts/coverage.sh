#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
coverage_build="${repo_root}/build/Testing-Coverage"
coverage_output="${coverage_build}/lcov.info"

cd "${repo_root}"
cmake --preset testing-coverage
cmake --build --preset testing-coverage

# Stale .gcda counters from a previous run would be captured as a union with
# this run's execution, so removed coverage could still appear as hit.
find "${coverage_build}" -name '*.gcda' -delete

ctest --preset testing-coverage

if command -v lcov >/dev/null 2>&1; then
  lcov --capture --directory "${coverage_build}" --output-file "${coverage_output}"
elif command -v gcovr >/dev/null 2>&1; then
  gcovr --root "${repo_root}" --object-directory "${coverage_build}" --lcov --output "${coverage_output}"
else
  echo "Coverage report tool missing; install one with: sudo apt install lcov" >&2
  exit 1
fi
