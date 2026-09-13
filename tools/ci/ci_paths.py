#!/usr/bin/env python3
"""Conservative job eligibility for #700; unknown paths and empty diffs run.

Globs match repository-relative paths (fnmatch's * includes slashes). Specific
may-skip exceptions precede broad run globs, except a job's own workflow always
runs. Git's quoted unusual filenames deliberately fall back to running.
"""

import argparse
from fnmatch import fnmatchcase
import json
from pathlib import Path
import sys


JOBS = {
    "toolbox-build": {
        "workflow": ".github/workflows/toolbox.yml",
        "run": ("common/*", "apple/toolbox/*", "example/*", "tests/*",
                "tools/ci/retro68_*", "cmake/*", "CMakeLists.txt"),
        "may_skip": ("docs/*", "plans/*", "win32/*", "apple/macos/*",
                     ".github/workflows/*.yml"),
    },
    "macos": {
        "workflow": ".github/workflows/macos.yml",
        "run": ("common/*", "apple/*", "example/*", "tests/*",
                "cmake/*", "CMakeLists.txt"),
        # Toolbox-only exceptions to apple/*; other workflows cannot change
        # this job. Shared tests and build inputs still run it (including #698).
        "may_skip": ("docs/*", "win32/*", "apple/toolbox/*",
                     "tools/ci/retro68_*", ".github/workflows/*.yml"),
    },
    "win32": {
        "workflow": ".github/workflows/windows.yml",
        "run": ("common/*", "win32/*", "example/*", "tests/*",
                "cmake/*", "CMakeLists.txt"),
        "may_skip": ("docs/*", "apple/*", "tools/ci/retro68_*",
                     ".github/workflows/*.yml"),
    },
}


def classify(job, paths):
    """Return (run, reason) for a complete diff; never infer safety by suffix."""
    policy = JOBS[job]
    if not paths:
        return True, "empty diff; run conservatively"
    if policy["workflow"] in paths:
        return True, "own workflow " + policy["workflow"]
    skipped = set()
    for path in paths:
        match = next((glob for glob in policy["may_skip"]
                      if fnmatchcase(path, glob)), None)
        if match is not None:
            skipped.add(match)
            continue
        match = next((glob for glob in policy["run"]
                      if fnmatchcase(path, glob)), None)
        # Quote path data so CLI/GITHUB_OUTPUT records always stay on one line.
        reason = json.dumps(path, ensure_ascii=True)
        if match is not None:
            return True, reason + " matches must-run " + match
        return True, reason + " is outside may-skip paths; run conservatively"
    return False, "all changed paths match may-skip: " + ", ".join(sorted(skipped))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--job", action="append", choices=JOBS, required=True)
    parser.add_argument("--summary", type=Path,
                        help="append one summary line per skipped job")
    parser.add_argument("paths", nargs="*", help="paths; default: newline stdin")
    args = parser.parse_args()
    paths = args.paths if args.paths else sys.stdin.read().splitlines()
    for job in args.job:
        run, reason = classify(job, paths)
        print("job=" + job)
        print("run=" + str(run).lower())
        print("reason=" + reason)
        if not run and args.summary is not None:
            with args.summary.open("a", encoding="utf-8") as summary:
                summary.write(job + " skipped: " + reason + "\n")


if __name__ == "__main__":
    main()
