#!/usr/bin/env python3
"""Refuse package staging that disagrees with a scenario fixture registry."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
from typing import Sequence


FIXTURE_PATTERN = re.compile(
    r"^([a-z0-9][a-z0-9-]*) corrupt-bag=([0-9]+)$"
)


class PackageFixtureError(RuntimeError):
    pass


def read_fixtures(path: pathlib.Path) -> tuple[tuple[str, int], ...]:
    try:
        raw_lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise PackageFixtureError(
            f"cannot read package fixture registry {path}: {error}"
        ) from error
    fixtures = []
    scenarios = set()
    for number, line in enumerate(raw_lines, 1):
        match = FIXTURE_PATTERN.fullmatch(line)
        if match is None:
            raise PackageFixtureError(
                f"{path}:{number}: invalid package fixture registry entry"
            )
        scenario = match.group(1)
        if scenario in scenarios:
            raise PackageFixtureError(
                f"{path}:{number}: duplicate package fixture scenario '{scenario}'"
            )
        scenarios.add(scenario)
        fixtures.append((scenario, int(match.group(2))))
    return tuple(fixtures)


def corrupt_bag_for(
    fixtures: tuple[tuple[str, int], ...], scenario: str
) -> int | None:
    for fixture_scenario, bag in fixtures:
        if fixture_scenario == scenario:
            return bag
    return None


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


def verify_staged(
    source: pathlib.Path,
    staged: pathlib.Path,
    bag: int | None,
    scenario: str | None = None,
) -> None:
    scenario_description = (
        f"scenario '{scenario}'" if scenario is not None else "scenario"
    )
    if not staged.is_file():
        if bag is not None:
            raise PackageFixtureError(
                f"{scenario_description} declares corrupt-bag={bag} but {staged} does not exist; "
                "this rail did not stage the fixture"
            )
        raise PackageFixtureError(
            f"{scenario_description} declares no package fixture but {staged} does not exist; "
            "this rail did not stage the package"
        )
    try:
        identical = _files_identical(source, staged)
    except OSError as error:
        raise PackageFixtureError(
            f"cannot compare staged package {staged} with source {source}: {error}"
        ) from error
    if bag is not None and identical:
        raise PackageFixtureError(
            f"{scenario_description} declares corrupt-bag={bag} but {staged} is "
            f"byte-identical to {source}; this rail did not stage the fixture"
        )
    if bag is None and not identical:
        raise PackageFixtureError(
            f"{scenario_description} declares no package fixture but {staged} differs from {source}"
        )


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    plan = subparsers.add_parser("plan")
    plan.add_argument("--registry", required=True, type=pathlib.Path)
    plan.add_argument("--scenario", required=True)

    verify = subparsers.add_parser("verify")
    verify.add_argument("--registry", required=True, type=pathlib.Path)
    verify.add_argument("--scenario", required=True)
    verify.add_argument("--source", required=True, type=pathlib.Path)
    verify.add_argument("--staged", required=True, type=pathlib.Path)
    return parser


def main(arguments: Sequence[str]) -> int:
    args = _build_parser().parse_args(arguments)
    try:
        bag = corrupt_bag_for(read_fixtures(args.registry), args.scenario)
        if args.command == "plan":
            if bag is not None:
                print(bag)
        else:
            verify_staged(args.source, args.staged, bag, args.scenario)
    except (PackageFixtureError, OSError) as error:
        print(f"Package fixture refused: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
