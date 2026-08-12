#!/usr/bin/env python3
"""Build and run one exact Loka commit through the Toolbox MAME rail."""

from __future__ import annotations

import argparse
import dataclasses
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Optional, Sequence


sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))

from loka_rig_common import (
    RIG_ID_PATTERN,
    SUPPORTED_ARTIFACT_CONTRACT_VERSION,
    SUPPORTED_DESCRIPTOR_VERSION,
    CommandLog,
    RigError,
    RunProgress,
    RunResult,
    execute_adapter,
    make_run_id,
    parse_bool,
    read_single_section,
    require_keys,
    resolve_commit,
    target_retained_value,
    write_manifest,
)


SUPPORTED_MODES = frozenset(("flow",))
SCENARIO_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")


@dataclasses.dataclass(frozen=True)
class RigDescriptor:
    rig_id: str
    machine: str
    build_architecture: str
    build_profile: str
    supported_modes: frozenset[str]
    disposable_for_input: bool
    capture_adapter: str
    recording_adapter: str
    artifact_contract_version: str


@dataclasses.dataclass(frozen=True)
class LocalMapping:
    archive_root: pathlib.Path
    mame_env_file: pathlib.Path
    golden_root: pathlib.Path


def _narrow_absolute_path(value: str, field: str, path: pathlib.Path) -> pathlib.Path:
    resolved = pathlib.Path(value.strip()).expanduser()
    if not resolved.is_absolute() or str(resolved) == "/":
        raise RigError("configuration", f"{path}: {field} must be a narrow absolute path")
    return resolved


def load_descriptor(path: pathlib.Path) -> RigDescriptor:
    section = read_single_section(path, "rig")
    expected = {
        "descriptor_version",
        "rig_id",
        "machine",
        "build_architecture",
        "build_profile",
        "supported_modes",
        "disposable_for_input",
        "capture_adapter",
        "recording_adapter",
        "artifact_contract_version",
    }
    require_keys(section, expected, path)
    if section["descriptor_version"] != SUPPORTED_DESCRIPTOR_VERSION:
        raise RigError("configuration", f"{path}: unsupported descriptor_version")
    if section["artifact_contract_version"] != SUPPORTED_ARTIFACT_CONTRACT_VERSION:
        raise RigError("configuration", f"{path}: unsupported artifact_contract_version")
    rig_id = section["rig_id"].strip()
    if not RIG_ID_PATTERN.fullmatch(rig_id):
        raise RigError("configuration", f"{path}: invalid rig_id")
    modes = frozenset(part.strip() for part in section["supported_modes"].split(",") if part.strip())
    if not modes or not modes.issubset(SUPPORTED_MODES):
        raise RigError("configuration", f"{path}: unsupported or empty supported_modes")
    if section["build_profile"] != "retro68-68k-release":
        raise RigError("configuration", f"{path}: unsupported build_profile")
    return RigDescriptor(
        rig_id=rig_id,
        machine=section["machine"].strip(),
        build_architecture=section["build_architecture"].strip(),
        build_profile=section["build_profile"].strip(),
        supported_modes=modes,
        disposable_for_input=parse_bool(section["disposable_for_input"], "disposable_for_input", path),
        capture_adapter=section["capture_adapter"].strip(),
        recording_adapter=section["recording_adapter"].strip(),
        artifact_contract_version=section["artifact_contract_version"].strip(),
    )


def load_local_mapping(path: pathlib.Path) -> LocalMapping:
    section = read_single_section(path, "local")
    require_keys(section, {"archive_root", "mame_env_file", "golden_root"}, path)
    return LocalMapping(
        archive_root=_narrow_absolute_path(section["archive_root"], "archive_root", path),
        mame_env_file=_narrow_absolute_path(section["mame_env_file"], "mame_env_file", path),
        golden_root=_narrow_absolute_path(section["golden_root"], "golden_root", path),
    )


def _read_scenarios(checkout: pathlib.Path) -> tuple[str, ...]:
    registry = checkout / "tests" / "toolbox" / "scrapbook-scenarios.txt"
    try:
        scenarios = tuple(registry.read_text(encoding="utf-8").splitlines())
    except OSError as error:
        raise RigError("golden-preflight", f"cannot read scenario registry: {registry}") from error
    if not scenarios or any(not SCENARIO_PATTERN.fullmatch(scenario) for scenario in scenarios):
        raise RigError("golden-preflight", f"invalid or empty scenario registry: {registry}")
    if len(set(scenarios)) != len(scenarios):
        raise RigError("golden-preflight", f"duplicate scenario registry entry: {registry}")
    return scenarios


def stage_goldens(checkout: pathlib.Path, golden_root: pathlib.Path) -> tuple[str, ...]:
    scenarios = _read_scenarios(checkout)
    destination = checkout / "build" / "mame-scenario" / "golden"
    temporary = destination.with_name("golden.tmp")
    try:
        for scenario in scenarios:
            source = golden_root / f"{scenario}.png"
            if not source.is_file() or source.is_symlink():
                raise RigError("golden-preflight", f"missing finalized rig-local golden: {source}")
        temporary.mkdir(parents=True, exist_ok=False)
        for scenario in scenarios:
            source = golden_root / f"{scenario}.png"
            shutil.copy2(source, temporary / source.name)
        temporary.replace(destination)
    except RigError:
        raise
    except OSError as error:
        raise RigError("golden-preflight", f"could not stage rig-local goldens: {error}") from error
    return scenarios


class ToolboxRigRun:
    def __init__(
        self,
        repo: pathlib.Path,
        descriptor: RigDescriptor,
        mapping: LocalMapping,
        requested_ref: str,
        mode: str,
    ):
        self.repo = repo
        self.descriptor = descriptor
        self.mapping = mapping
        self.requested_ref = requested_ref
        self.mode = mode
        self.progress = RunProgress()
        self.commit_sha = ""
        self.run_id = ""
        self.archive: Optional[pathlib.Path] = None
        self.checkout: Optional[pathlib.Path] = None
        self.target_state = "not-created"
        self.scenarios: tuple[str, ...] = ()
        self.command_log: Optional[CommandLog] = None

    def _run(
        self,
        arguments: Sequence[str],
        stage: str,
        *,
        cwd: Optional[pathlib.Path] = None,
        environment: Optional[dict[str, str]] = None,
        output: Optional[object] = None,
        timeout: int = 1800,
    ) -> subprocess.CompletedProcess[bytes]:
        try:
            return subprocess.run(
                arguments,
                check=True,
                cwd=cwd,
                env=environment,
                stdout=output,
                stderr=subprocess.STDOUT if output is not None else subprocess.PIPE,
                timeout=timeout,
            )
        except subprocess.CalledProcessError as error:
            raise RigError(stage, f"{arguments[0]} command exited {error.returncode}") from error
        except subprocess.TimeoutExpired as error:
            raise RigError(stage, f"{arguments[0]} command timed out") from error
        except OSError as error:
            raise RigError(stage, f"could not execute {arguments[0]}: {error}") from error

    def _logged_run(
        self,
        arguments: Sequence[str],
        log_name: str,
        stage: str,
        *,
        environment: Optional[dict[str, str]] = None,
    ) -> None:
        assert self.archive is not None
        assert self.checkout is not None
        with (self.archive / log_name).open("ab") as output:
            self._run(arguments, stage, cwd=self.checkout, environment=environment, output=output)

    def _configured_machine(self) -> str:
        command = (
            "set -a; . \"$1\"; set +a; printf '%s\\n' \"${MAME_MACHINE:-maciici}\""
        )
        try:
            result = subprocess.run(
                ("/bin/bash", "-c", command, "loka-rig-machine", str(self.mapping.mame_env_file)),
                check=True,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=30,
            )
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as error:
            raise RigError("mame-preflight", "could not read MAME machine from local environment") from error
        return result.stdout.strip()

    def prepare(self) -> None:
        self.commit_sha = resolve_commit(self.repo, self.requested_ref)
        self.run_id = make_run_id(self.commit_sha, self.mode, "scrapbook")
        self.archive = self.mapping.archive_root / self.descriptor.rig_id / self.run_id
        self.checkout = self.repo / "build" / "loka-rig" / "checkouts" / self.run_id
        try:
            self.archive.mkdir(parents=True, exist_ok=False)
            self.checkout.parent.mkdir(parents=True, exist_ok=True)
        except OSError as error:
            raise RigError("checkout", f"could not create run directories: {error}") from error
        self.command_log = CommandLog(self.archive / "orchestrator.log")
        self.command_log.write(f"Resolved {self.requested_ref} to {self.commit_sha}")
        if self.mode not in self.descriptor.supported_modes:
            raise RigError("configuration", f"{self.descriptor.rig_id} does not support {self.mode}")
        if not self.mapping.mame_env_file.is_file():
            raise RigError("mame-preflight", f"MAME environment file not found: {self.mapping.mame_env_file}")
        configured_machine = self._configured_machine()
        if configured_machine != self.descriptor.machine:
            raise RigError(
                "mame-preflight",
                f"descriptor machine {self.descriptor.machine} does not match configured MAME machine {configured_machine}",
            )
        self._run(
            ("git", "-C", str(self.repo), "worktree", "add", "--detach", str(self.checkout), self.commit_sha),
            "checkout",
        )
        self.target_state = "retained"
        self.scenarios = stage_goldens(self.checkout, self.mapping.golden_root)

    def build(self) -> None:
        assert self.checkout is not None
        self._logged_run(
            ("cmake", "--preset", self.descriptor.build_profile),
            "build.log",
            "configure",
        )
        self._logged_run(
            (
                "cmake",
                "--build",
                "--preset",
                self.descriptor.build_profile,
                "--target",
                "LokaTestsToolbox68K_APPL",
                "--parallel",
                "2",
            ),
            "build.log",
            "build",
        )

    def run_runtime(self) -> None:
        assert self.checkout is not None
        environment = os.environ.copy()
        environment["MAME_ENV_FILE"] = str(self.mapping.mame_env_file)
        environment["LOKA_TOOLBOX_PRESENTATION_RUN_ID"] = self.run_id
        self._logged_run(
            ("/bin/bash", "tests/toolbox/run-presentation-rail.sh"),
            "runner.log",
            "runtime",
            environment=environment,
        )
        presentation = self.checkout / "build" / "mame-scenario" / "presentation" / self.run_id
        if not (presentation / "presentation-manifest.txt").is_file():
            raise RigError("runtime", "Toolbox rail did not finalize its presentation manifest")

    def collect(self) -> None:
        assert self.archive is not None
        assert self.checkout is not None
        presentation = self.checkout / "build" / "mame-scenario" / "presentation" / self.run_id
        required = ("presentation-manifest.txt",) + tuple(f"{scenario}.png" for scenario in self.scenarios)
        missing = [name for name in required if not (presentation / name).is_file()]
        if missing:
            raise RigError("artifact-collection", "missing artifacts: " + ", ".join(missing))
        try:
            for name in required:
                shutil.copy2(presentation / name, self.archive / name)
        except OSError as error:
            raise RigError("artifact-collection", f"could not collect Toolbox artifacts: {error}") from error

    def best_effort_collect(self) -> None:
        if self.archive is None or self.checkout is None or not self.run_id:
            return
        incomplete = self.checkout / "build" / "mame-scenario" / "presentation" / f"{self.run_id}.incomplete"
        if not incomplete.is_dir():
            return
        try:
            shutil.copytree(incomplete, self.archive / "presentation.incomplete", dirs_exist_ok=True)
        except OSError:
            pass

    def note_failure(self) -> None:
        if self.command_log:
            self.command_log.write(
                f"FAILED at {self.progress.failure_stage}: {self.progress.failure_message}"
            )

    def finalize_manifest(self, result: str) -> None:
        assert self.archive is not None
        target_workdir = str(self.checkout) if self.checkout is not None else "not-created"
        common_result = RunResult.from_progress(
            adapter="toolbox",
            rig_id=self.descriptor.rig_id,
            requested_ref=self.requested_ref,
            commit_sha=self.commit_sha,
            mode=self.mode,
            result=result,
            progress=self.progress,
            recording_status="manual",
            target_retained=target_retained_value(self.target_state),
            target_workdir=target_workdir,
            next_diagnostic_command="inspect retained Toolbox checkout" if result != "passed" else "none",
        )
        adapter_fields = (
            ("descriptor_version", SUPPORTED_DESCRIPTOR_VERSION),
            ("artifact_contract_version", self.descriptor.artifact_contract_version),
            ("machine", self.descriptor.machine),
            ("build_architecture", self.descriptor.build_architecture),
            ("build_profile", self.descriptor.build_profile),
            ("scenario_count", str(len(self.scenarios))),
            ("capture_adapter", self.descriptor.capture_adapter),
            ("recording_adapter", self.descriptor.recording_adapter),
        )
        write_manifest(self.archive, common_result, adapter_fields)

    def cleanup_success(self) -> None:
        assert self.checkout is not None
        self._run(
            ("git", "-C", str(self.repo), "worktree", "remove", "--force", str(self.checkout)),
            "cleanup",
        )
        self.target_state = "removed"

    def execute(self) -> pathlib.Path:
        return execute_adapter(self)


def run_rig(
    *,
    repo: pathlib.Path,
    rig_id: str,
    requested_ref: str,
    mode: str,
    local_config: Optional[pathlib.Path],
) -> pathlib.Path:
    descriptor_path = pathlib.Path(__file__).resolve().parent / "rigs" / f"{rig_id}.ini"
    local_path = local_config
    if local_path is None:
        configured = os.environ.get("LOKA_RIG_LOCAL_CONFIG")
        local_path = pathlib.Path(configured) if configured else pathlib.Path.home() / ".config" / "loka" / "rigs" / f"{rig_id}.ini"
    descriptor = load_descriptor(descriptor_path)
    mapping = load_local_mapping(local_path)
    return ToolboxRigRun(repo, descriptor, mapping, requested_ref, mode).execute()


def parse_args(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run", help="build and run one specified commit")
    run.add_argument("rig_id")
    run.add_argument("--ref", required=True)
    run.add_argument("--mode", required=True, choices=sorted(SUPPORTED_MODES))
    run.add_argument("--local-config", type=pathlib.Path)
    return parser.parse_args(arguments)


def main(arguments: Sequence[str]) -> int:
    args = parse_args(arguments)
    repo = pathlib.Path(__file__).resolve().parents[2]
    try:
        archive = run_rig(
            repo=repo,
            rig_id=args.rig_id,
            requested_ref=args.ref,
            mode=args.mode,
            local_config=args.local_config,
        )
    except RigError as error:
        print(f"{error.stage} stage failed: {error}", file=sys.stderr)
        return 1
    print(f"Toolbox rig passed: {archive}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
