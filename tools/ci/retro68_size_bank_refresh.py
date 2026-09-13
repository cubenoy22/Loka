#!/usr/bin/env python3
"""Refresh a Release size bank, or check total-growth headroom without writing it.

Refresh reads interface provenance from the Release build's CMake cache.
Exit 2 means low headroom; measurement/refresh failures exit 1.
"""

import argparse
import copy
import json
import pathlib
import re
import subprocess
import sys
import tempfile

import retro68_size_report as size_report


REPO = pathlib.Path(__file__).resolve().parents[2]


def measure(build_root, baseline):
    """Read the bank-owned inventory using the report's resource parser."""
    measurements = []
    for artifact in baseline["artifacts"]:
        path = build_root / artifact["path"]
        resources = size_report.resource_payload_sizes(path)
        measurements.append(dict(total=path.stat().st_size, **resources))
    return measurements


def verify_zero(build_root, baseline):
    """The ordinary gate allows growth; a refreshed bank must match exactly."""
    if size_report.report(build_root, baseline) != 0 or any(
        current != artifact["baseline"]
        for artifact, current in zip(baseline["artifacts"], measure(build_root, baseline))
    ):
        raise size_report.SizeReportError("refreshed bank has non-zero deltas")


def git(*args):
    return subprocess.check_output(["git", "-C", str(REPO), *args], text=True).strip()


def refresh(build_root, baseline_path, baseline, body_path):
    cache = (build_root / "CMakeCache.txt").read_text(encoding="utf-8").splitlines()
    if "CMAKE_BUILD_TYPE:STRING=Release" not in cache or "RETRO68_CPU:STRING=m68k" not in cache:
        raise size_report.SizeReportError("refresh requires a Retro68 m68k Release build")
    if "LOKA_TOOLBOX_MULTIVERSAL_INTERFACES:BOOL=ON" in cache:
        label = "Multiversal Interfaces"
    elif "LOKA_TOOLBOX_MULTIVERSAL_INTERFACES:BOOL=OFF" in cache:
        label = "local Universal Interfaces"
    else:
        raise size_report.SizeReportError("cannot determine build interface provenance")
    old_commit = baseline["identity"]["source_commit"]
    head = git("rev-parse", "HEAD")
    subjects = git("log", "--format=%s", "%s..%s" % (old_commit, head))
    prs = sorted(set(re.findall(r"\(#(\d+)\)$", subjects, re.MULTILINE)), key=int)
    accumulated = ", ".join("#" + number for number in prs) or "no merged PR suffixes"
    candidate = copy.deepcopy(baseline)
    candidate["identity"]["source_commit"] = head
    candidate["identity"]["component_reference"] = (
        "%s build (main %s full Release refresh; accumulated %s since the previous %s identity)"
        % (label, head[:8], accumulated, old_commit[:8])
    )
    lines = [
        "Bank refresh only, no code changes.", "",
        candidate["identity"]["component_reference"], "",
        "| App | Old bank | Measured (new bank) | Delta absorbed |",
        "|---|---:|---:|---:|",
    ]
    for artifact, current in zip(candidate["artifacts"], measure(build_root, baseline)):
        old_total = artifact["baseline"]["total"]
        lines.append("| %s | %d | %d | %+d |" % (
            artifact["name"], old_total, current["total"], current["total"] - old_total
        ))
        artifact["baseline"] = current

    # Validate the serialized candidate before replacing live bank truth.
    with tempfile.TemporaryDirectory(dir=baseline_path.parent) as directory:
        temporary = pathlib.Path(directory) / baseline_path.name
        temporary.write_text(json.dumps(candidate, indent=2) + "\n", encoding="utf-8")
        verify_zero(build_root, size_report.load_baseline(temporary))
        temporary.replace(baseline_path)
    verify_zero(build_root, size_report.load_baseline(baseline_path))
    lines.extend([
        "", "All %d banked `*_APPL` targets rebuilt with `retro68-68k-release`; "
        "all %d total/CODE/DATA/RELA deltas are zero. Schema, artifact paths, "
        "and the %s B material-growth allowance are unchanged." % (
            len(candidate["artifacts"]), len(candidate["artifacts"]) * 4,
            format(baseline["material_growth_bytes"], ",")),
        "", "Automation uses `GITHUB_TOKEN` with `contents: write` and "
        "`pull-requests: write`. If PR creation is blocked, enable repository "
        "Settings → Actions → General → Workflow permissions → "
        "Allow GitHub Actions to create and approve pull requests "
        "(organization policy must also permit it). Merge remains human.",
        "", "Verification: build-verified only; no runtime verification claimed.",
    ])
    body = "\n".join(lines) + "\n"
    print(body)
    if body_path:
        body_path.write_text(body, encoding="utf-8")


def check_headroom(build_root, baseline, threshold):
    rows = []
    for artifact, current in zip(baseline["artifacts"], measure(build_root, baseline)):
        remaining = baseline["material_growth_bytes"] - (current["total"] - artifact["baseline"]["total"])
        if remaining < threshold:
            rows.append("| %s | %d |" % (artifact["name"], remaining))
    if not rows:
        print("All apps have at least %d bytes of remaining allowance." % threshold)
        return 0
    print("Retro68 68K apps with less than %d bytes of remaining allowance:\n" % threshold)
    print("| App | Remaining bytes |\n|---|---:|")
    print("\n".join(rows))
    return 2


def main(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_root", type=pathlib.Path)
    parser.add_argument("--baseline", type=pathlib.Path,
                        default=pathlib.Path(__file__).with_name("retro68_68k_size_baseline.json"))
    parser.add_argument("--check-headroom", type=int, metavar="N")
    parser.add_argument("--body", type=pathlib.Path, help="write the generated PR body here")
    try:
        options = parser.parse_args(arguments)
    except SystemExit as error:
        return 0 if error.code == 0 else 1
    try:
        baseline = size_report.load_baseline(options.baseline)
        if options.check_headroom is not None:
            if options.check_headroom < 0 or options.body:
                raise size_report.SizeReportError("headroom requires N >= 0 and no refresh options")
            return check_headroom(options.build_root, baseline, options.check_headroom)
        refresh(options.build_root, options.baseline, baseline, options.body)
        return 0
    except (OSError, KeyError, subprocess.CalledProcessError, size_report.SizeReportError) as error:
        print("retro68_size_bank_refresh: %s" % error, file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
