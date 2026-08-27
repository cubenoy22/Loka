#!/usr/bin/env python3
"""Refuse accidental settled-golden identity with an example's startup cell."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Sequence


CELL_PART_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
STARTUP_SCENARIO = "startup"


class GoldenIdentityError(RuntimeError):
    pass


def read_cells(path: pathlib.Path, description: str) -> tuple[tuple[str, str], ...]:
    try:
        raw_lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise GoldenIdentityError(f"cannot read {description} {path}: {error}") from error
    cells = []
    for number, line in enumerate(raw_lines, 1):
        parts = line.split()
        if (
            len(parts) != 2
            or not CELL_PART_PATTERN.fullmatch(parts[0])
            or not CELL_PART_PATTERN.fullmatch(parts[1])
        ):
            raise GoldenIdentityError(f"{path}:{number}: invalid {description} entry")
        cells.append((parts[0], parts[1]))
    if len(set(cells)) != len(cells):
        raise GoldenIdentityError(f"duplicate {description} entry: {path}")
    return tuple(cells)


def read_declarations(
    path: pathlib.Path,
) -> tuple[tuple[tuple[str, str], str], ...]:
    """Read declared identities, each of which must say why it is expected.

    An identity nobody can explain is the defect this guard exists to catch, so
    the reason is a field of the entry rather than a comment beside it.
    """
    try:
        raw_lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise GoldenIdentityError(
            f"cannot read startup-identity declaration {path}: {error}"
        ) from error
    entries = []
    for number, line in enumerate(raw_lines, 1):
        parts = line.split(maxsplit=2)
        if (
            len(parts) < 2
            or not CELL_PART_PATTERN.fullmatch(parts[0])
            or not CELL_PART_PATTERN.fullmatch(parts[1])
        ):
            raise GoldenIdentityError(
                f"{path}:{number}: invalid startup-identity declaration entry"
            )
        reason = parts[2].strip() if len(parts) > 2 else ""
        if not reason:
            raise GoldenIdentityError(
                f"{path}:{number}: startup-identity declaration for "
                f"{parts[0]} {parts[1]} states no reason"
            )
        entries.append(((parts[0], parts[1]), reason))
    cells = [cell for cell, _ in entries]
    if len(set(cells)) != len(cells):
        raise GoldenIdentityError(
            f"duplicate startup-identity declaration entry: {path}"
        )
    return tuple(entries)


def read_contract(
    registry: pathlib.Path, declarations: pathlib.Path
) -> tuple[tuple[tuple[str, str], ...], frozenset[tuple[str, str]]]:
    scenarios = read_cells(registry, "scenario registry")
    if not scenarios:
        raise GoldenIdentityError(f"scenario registry is empty: {registry}")
    declared = tuple(cell for cell, _ in read_declarations(declarations))
    scenario_set = frozenset(scenarios)
    for cell in declared:
        if cell not in scenario_set:
            raise GoldenIdentityError(
                f"startup-identity declaration is not registered: {cell[0]} {cell[1]}"
            )
        if cell[1] == STARTUP_SCENARIO:
            raise GoldenIdentityError(
                f"startup cell cannot declare identity with itself: {cell[0]} {cell[1]}"
            )
        if (cell[0], STARTUP_SCENARIO) not in scenario_set:
            raise GoldenIdentityError(
                f"declared cell has no registered startup cell: {cell[0]} {cell[1]}"
            )
    return scenarios, frozenset(declared)


def _require_regular_capture(path: pathlib.Path, description: str) -> None:
    if not path.is_file() or path.is_symlink():
        raise GoldenIdentityError(f"{description} is not a regular file: {path}")


def _files_identical(first: pathlib.Path, second: pathlib.Path) -> bool:
    if first.stat().st_size != second.stat().st_size:
        return False
    with first.open("rb") as first_file, second.open("rb") as second_file:
        while True:
            first_block = first_file.read(1024 * 1024)
            second_block = second_file.read(1024 * 1024)
            if first_block != second_block:
                return False
            if not first_block:
                return True


def _verify_relationship(
    example: str,
    scenario: str,
    capture: pathlib.Path,
    startup: pathlib.Path,
    declared: frozenset[tuple[str, str]],
) -> None:
    _require_regular_capture(capture, "settled capture")
    _require_regular_capture(startup, "startup golden")
    cell = (example, scenario)
    identical = _files_identical(capture, startup)
    if identical and cell not in declared:
        raise GoldenIdentityError(
            f"settled capture for {example} {scenario} is byte-identical to "
            f"{example} startup but the identity is not declared"
        )
    if not identical and cell in declared:
        raise GoldenIdentityError(
            f"stale startup-identity declaration for {example} {scenario}: "
            f"settled capture differs from {example} startup"
        )


def verify_candidate_recording(
    registry: pathlib.Path,
    declarations: pathlib.Path,
    golden_root: pathlib.Path,
    capture: pathlib.Path,
    example: str,
    scenario: str,
) -> None:
    scenarios, declared = read_contract(registry, declarations)
    if (example, scenario) not in frozenset(scenarios):
        raise GoldenIdentityError(f"scenario is not registered: {example} {scenario}")
    _require_regular_capture(capture, "settled capture")
    if scenario != STARTUP_SCENARIO:
        startup = golden_root / example / f"{STARTUP_SCENARIO}.png"
        if not startup.is_file() or startup.is_symlink():
            raise GoldenIdentityError(
                f"startup golden is unavailable for {example} {scenario}; "
                f"record {example} {STARTUP_SCENARIO} first"
            )
        _verify_relationship(example, scenario, capture, startup, declared)
        return

    for sibling_example, sibling_scenario in scenarios:
        if sibling_example != example or sibling_scenario == STARTUP_SCENARIO:
            continue
        sibling = golden_root / example / f"{sibling_scenario}.png"
        if sibling.exists() or sibling.is_symlink():
            _verify_relationship(example, sibling_scenario, sibling, capture, declared)


def verify_recorded_set(
    registry: pathlib.Path,
    declarations: pathlib.Path,
    golden_root: pathlib.Path,
) -> None:
    scenarios, declared = read_contract(registry, declarations)
    for example, scenario in scenarios:
        capture = golden_root / example / f"{scenario}.png"
        _require_regular_capture(capture, "recorded settled golden")
        if scenario == STARTUP_SCENARIO:
            continue
        startup = golden_root / example / f"{STARTUP_SCENARIO}.png"
        if not startup.is_file() or startup.is_symlink():
            raise GoldenIdentityError(
                f"startup golden is unavailable for {example} {scenario}; "
                f"record {example} {STARTUP_SCENARIO} first"
            )
        _verify_relationship(example, scenario, capture, startup, declared)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", required=True, type=pathlib.Path)
    parser.add_argument("--declarations", required=True, type=pathlib.Path)
    parser.add_argument("--golden-root", required=True, type=pathlib.Path)
    parser.add_argument("--capture", required=True, type=pathlib.Path)
    parser.add_argument("--example", required=True)
    parser.add_argument("--scenario", required=True)
    return parser


def main(arguments: Sequence[str]) -> int:
    args = _build_parser().parse_args(arguments)
    try:
        verify_candidate_recording(
            args.registry,
            args.declarations,
            args.golden_root,
            args.capture,
            args.example,
            args.scenario,
        )
    except (GoldenIdentityError, OSError) as error:
        print(f"Golden startup identity refused: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
