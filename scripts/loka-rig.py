#!/usr/bin/env python3
"""Run one exact Loka commit through a registered verification rig."""

from __future__ import annotations

import argparse
import dataclasses
import importlib
import pathlib
import sys
from typing import Optional, Sequence

from loka_rig_common import RIG_ID_PATTERN, RigError, SUPPORTED_PUBLIC_MODES


@dataclasses.dataclass(frozen=True)
class AdapterRegistration:
    name: str
    descriptor_directory: pathlib.PurePosixPath
    module: str


ADAPTERS = (
    AdapterRegistration("macos", pathlib.PurePosixPath("scripts/macos/rigs"), "macos.loka_macos_rig"),
    AdapterRegistration("toolbox", pathlib.PurePosixPath("scripts/toolbox/rigs"), "toolbox.loka_toolbox_rig"),
)


def find_adapter(repo: pathlib.Path, rig_id: str) -> AdapterRegistration:
    if not RIG_ID_PATTERN.fullmatch(rig_id):
        raise RigError("configuration", f"invalid rig ID: {rig_id}")
    matches = [
        adapter
        for adapter in ADAPTERS
        if (repo / adapter.descriptor_directory / f"{rig_id}.ini").is_file()
    ]
    if not matches:
        raise RigError("configuration", f"unknown rig: {rig_id}")
    if len(matches) != 1:
        raise RigError("configuration", f"rig is registered by multiple adapters: {rig_id}")
    return matches[0]


def parse_args(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run", help="build and run one specified commit")
    run.add_argument("rig_id")
    run.add_argument("--ref", dest="requested_ref", required=True)
    run.add_argument("--mode", required=True, choices=sorted(SUPPORTED_PUBLIC_MODES))
    run.add_argument("--local-config", type=pathlib.Path)
    return parser.parse_args(arguments)


def run_adapter(
    repo: pathlib.Path,
    registration: AdapterRegistration,
    rig_id: str,
    requested_ref: str,
    mode: str,
    local_config: Optional[pathlib.Path],
) -> pathlib.Path:
    module = importlib.import_module(registration.module)
    return module.run_rig(
        repo=repo,
        rig_id=rig_id,
        requested_ref=requested_ref,
        mode=mode,
        local_config=local_config,
    )


def main(arguments: Sequence[str]) -> int:
    args = parse_args(arguments)
    repo = pathlib.Path(__file__).resolve().parent.parent
    try:
        registration = find_adapter(repo, args.rig_id)
        archive = run_adapter(
            repo,
            registration,
            args.rig_id,
            args.requested_ref,
            args.mode,
            args.local_config,
        )
    except RigError as error:
        print(f"{error.stage} stage failed: {error}", file=sys.stderr)
        return 1
    print(f"loka-rig passed: {archive}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
