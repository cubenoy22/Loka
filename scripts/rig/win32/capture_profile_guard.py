#!/usr/bin/env python3
"""Refuse a Win32 capture whose environment is not the one the rig declares."""

from __future__ import annotations

import argparse
import configparser
import pathlib
import re
import sys
from typing import Sequence


PROFILE_LINE_PATTERN = re.compile(r"^([a-z_]+)=(.*)$")
CAPTURE_SECTION = "capture"


class CaptureProfileError(RuntimeError):
    pass


def read_profile(path: pathlib.Path) -> dict[str, str]:
    """Parse a scenario profile, refusing the shapes the runner also refuses."""
    try:
        raw_lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise CaptureProfileError(f"cannot read capture profile {path}: {error}") from error
    values: dict[str, str] = {}
    for number, line in enumerate(raw_lines, 1):
        matched = PROFILE_LINE_PATTERN.match(line)
        if not matched:
            raise CaptureProfileError(f"{path}:{number}: invalid profile line '{line}'")
        field = matched.group(1)
        if field in values:
            raise CaptureProfileError(f"{path}:{number}: duplicate profile field '{field}'")
        values[field] = matched.group(2)
    if not values:
        raise CaptureProfileError(f"capture profile is empty: {path}")
    return values


def read_declared_capture(path: pathlib.Path) -> dict[str, str]:
    """Read the [capture] section a rig descriptor declares."""
    parser = configparser.ConfigParser()
    try:
        with path.open(encoding="utf-8") as handle:
            parser.read_file(handle)
    except OSError as error:
        raise CaptureProfileError(f"cannot read rig descriptor {path}: {error}") from error
    except configparser.Error as error:
        raise CaptureProfileError(f"{path}: unreadable rig descriptor: {error}") from error
    if not parser.has_section(CAPTURE_SECTION):
        raise CaptureProfileError(f"{path}: rig descriptor declares no [{CAPTURE_SECTION}] section")
    declared = {key: value.strip() for key, value in parser.items(CAPTURE_SECTION)}
    if not declared:
        raise CaptureProfileError(f"{path}: [{CAPTURE_SECTION}] declares no fields")
    for field, value in declared.items():
        if not PROFILE_LINE_PATTERN.match(f"{field}={value}"):
            raise CaptureProfileError(f"{path}: invalid declared capture field '{field}'")
        if not value:
            raise CaptureProfileError(f"{path}: declared capture field '{field}' states no value")
    return declared


def verify_capture_profile(descriptor: pathlib.Path, profile: pathlib.Path) -> None:
    """Refuse unless every declared field is present and equal in the profile.

    Fields the profile carries but the descriptor omits are left alone: the
    descriptor says what the goldens are pinned to, not everything that can be
    observed. Widening or narrowing that set is a tracked edit.
    """
    declared = read_declared_capture(descriptor)
    reported = read_profile(profile)
    for field in sorted(declared):
        if field not in reported:
            raise CaptureProfileError(
                f"rig {descriptor.stem} declares {field}={declared[field]} "
                f"but the capture reports no such field"
            )
        if reported[field] != declared[field]:
            raise CaptureProfileError(
                f"rig {descriptor.stem} declares {field}={declared[field]} "
                f"but this machine reports {field}={reported[field]}"
            )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--descriptor", required=True, type=pathlib.Path)
    parser.add_argument("--profile", required=True, type=pathlib.Path)
    return parser


def main(arguments: Sequence[str]) -> int:
    args = _build_parser().parse_args(arguments)
    try:
        verify_capture_profile(args.descriptor, args.profile)
    except (CaptureProfileError, OSError) as error:
        print(f"Win32 capture profile refused: {error}", file=sys.stderr)
        return 4
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
