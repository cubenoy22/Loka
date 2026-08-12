#!/usr/bin/env python3
"""Shared host-owned lifecycle and result vocabulary for Loka rig adapters."""

from __future__ import annotations

import configparser
import dataclasses
import datetime
import hashlib
import pathlib
import re
import subprocess
from typing import Iterable, Optional, Protocol, Sequence


SUPPORTED_DESCRIPTOR_VERSION = "1"
SUPPORTED_ARTIFACT_CONTRACT_VERSION = "1"
SUPPORTED_PUBLIC_MODES = frozenset(("flow", "input", "inspect"))
RIG_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9.-]*$")


class RigError(RuntimeError):
    def __init__(self, stage: str, message: str):
        super().__init__(message)
        self.stage = stage


class CommandLog:
    def __init__(self, path: pathlib.Path):
        self._path = path

    def write(self, message: str) -> None:
        with self._path.open("a", encoding="utf-8") as output:
            output.write(f"[{utc_now()}] {message}\n")
        print(message, flush=True)


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


@dataclasses.dataclass
class RunProgress:
    """Mutable state owned by the common executor for exactly one rig run."""

    started_at: str = dataclasses.field(default_factory=utc_now)
    ended_at: str = ""
    failure_stage: str = ""
    failure_message: str = ""
    build_passed: bool = False
    runtime_passed: bool = False
    presentation_collected: bool = False
    manifest_finalized: bool = False

    def fail(self, stage: str, message: str) -> None:
        self.failure_stage = stage
        self.failure_message = message

    def finish(self) -> None:
        self.ended_at = utc_now()


@dataclasses.dataclass(frozen=True)
class RunResult:
    """Immutable common facts captured when an adapter finalizes its manifest."""

    adapter: str
    rig_id: str
    requested_ref: str
    commit_sha: str
    mode: str
    started_at: str
    ended_at: str
    result: str
    failure_stage: str
    failure_message: str
    build_verification: str
    runtime_verification: str
    machine_verdict: str
    presentation_status: str
    recording_status: str
    target_retained: str
    target_workdir: str
    next_diagnostic_command: str

    def __post_init__(self) -> None:
        if self.result not in ("passed", "failed"):
            raise RigError("manifest", f"invalid result: {self.result}")
        verification_values = {"passed", "failed-or-not-reached"}
        if self.build_verification not in verification_values:
            raise RigError("manifest", f"invalid build_verification: {self.build_verification}")
        if self.runtime_verification not in verification_values:
            raise RigError("manifest", f"invalid runtime_verification: {self.runtime_verification}")
        if self.machine_verdict not in verification_values:
            raise RigError("manifest", f"invalid machine_verdict: {self.machine_verdict}")
        if self.presentation_status not in ("collected", "failed-or-not-reached"):
            raise RigError("manifest", f"invalid presentation_status: {self.presentation_status}")
        if self.recording_status not in ("not-requested", "manual", "collected", "failed"):
            raise RigError("manifest", f"invalid recording_status: {self.recording_status}")
        if self.target_retained not in ("not-created", "0", "1"):
            raise RigError("manifest", f"invalid target_retained: {self.target_retained}")

    @classmethod
    def from_progress(
        cls,
        *,
        adapter: str,
        rig_id: str,
        requested_ref: str,
        commit_sha: str,
        mode: str,
        result: str,
        progress: RunProgress,
        recording_status: str,
        target_retained: str,
        target_workdir: str,
        next_diagnostic_command: str,
    ) -> "RunResult":
        runtime = "passed" if progress.runtime_passed else "failed-or-not-reached"
        return cls(
            adapter=adapter,
            rig_id=rig_id,
            requested_ref=requested_ref,
            commit_sha=commit_sha,
            mode=mode,
            started_at=progress.started_at,
            ended_at=progress.ended_at or utc_now(),
            result=result,
            failure_stage=progress.failure_stage or "none",
            failure_message=progress.failure_message or "none",
            build_verification="passed" if progress.build_passed else "failed-or-not-reached",
            runtime_verification=runtime,
            machine_verdict=runtime,
            presentation_status="collected" if progress.presentation_collected else "failed-or-not-reached",
            recording_status=recording_status,
            target_retained=target_retained,
            target_workdir=target_workdir,
            next_diagnostic_command=next_diagnostic_command,
        )

    def manifest_fields(self) -> tuple[tuple[str, str], ...]:
        return (
            ("adapter", self.adapter),
            ("rig_id", self.rig_id),
            ("requested_ref", self.requested_ref),
            ("commit_sha", self.commit_sha),
            ("mode", self.mode),
            ("started_at", self.started_at),
            ("ended_at", self.ended_at),
            ("result", self.result),
            ("failure_stage", self.failure_stage),
            ("failure_message", self.failure_message),
            ("build_verification", self.build_verification),
            ("runtime_verification", self.runtime_verification),
            ("machine_verdict", self.machine_verdict),
            ("presentation_status", self.presentation_status),
            ("recording_status", self.recording_status),
            ("target_retained", self.target_retained),
            ("target_workdir", self.target_workdir),
            ("next_diagnostic_command", self.next_diagnostic_command),
        )


class RigAdapter(Protocol):
    progress: RunProgress
    archive: Optional[pathlib.Path]

    def prepare(self) -> None: ...

    def build(self) -> None: ...

    def run_runtime(self) -> None: ...

    def collect(self) -> None: ...

    def best_effort_collect(self) -> None: ...

    def note_failure(self) -> None: ...

    def finalize_manifest(self, result: str) -> None: ...

    def cleanup_success(self) -> None: ...


def _record_failure(adapter: RigAdapter, stage: str, message: str) -> None:
    adapter.progress.fail(stage, message)
    adapter.note_failure()


def _finalize_manifest(adapter: RigAdapter, result: str) -> str:
    try:
        adapter.finalize_manifest(result)
        adapter.progress.manifest_finalized = True
        return result
    except (OSError, RigError) as error:
        adapter.progress.fail("manifest", str(error))
        adapter.progress.manifest_finalized = False
        adapter.note_failure()
        return "failed"


def execute_adapter(adapter: RigAdapter) -> pathlib.Path:
    """Run the common sequence while the adapter owns platform mechanisms."""

    result = "failed"
    try:
        adapter.prepare()
        adapter.build()
        adapter.progress.build_passed = True
        adapter.run_runtime()
        adapter.progress.runtime_passed = True
        adapter.collect()
        adapter.progress.presentation_collected = True
        result = "passed"
    except RigError as error:
        _record_failure(adapter, error.stage, str(error))
        adapter.best_effort_collect()
    except KeyboardInterrupt:
        _record_failure(adapter, "interrupted", "run interrupted by operator")
        adapter.best_effort_collect()
    finally:
        adapter.progress.finish()
        if adapter.archive is not None:
            result = _finalize_manifest(adapter, result)
        if result == "passed" and adapter.progress.presentation_collected and adapter.progress.manifest_finalized:
            try:
                adapter.cleanup_success()
            except RigError as error:
                _record_failure(adapter, error.stage, str(error))
                result = "failed"
            adapter.progress.manifest_finalized = False
            result = _finalize_manifest(adapter, result)
    if result != "passed":
        raise RigError(adapter.progress.failure_stage or "run", adapter.progress.failure_message or "run failed")
    if adapter.archive is None:
        raise RigError("run", "adapter completed without an archive")
    return adapter.archive


def read_single_section(path: pathlib.Path, section: str) -> configparser.SectionProxy:
    parser = configparser.ConfigParser(interpolation=None)
    try:
        with path.open("r", encoding="utf-8") as source:
            parser.read_file(source)
    except (OSError, configparser.Error) as error:
        raise RigError("configuration", f"cannot read {path}: {error}") from error
    if parser.sections() != [section]:
        raise RigError("configuration", f"{path} must contain only [{section}]")
    return parser[section]


def require_keys(section: configparser.SectionProxy, expected: set[str], path: pathlib.Path) -> None:
    actual = set(section.keys())
    missing = sorted(expected - actual)
    unknown = sorted(actual - expected)
    if missing or unknown:
        detail = []
        if missing:
            detail.append("missing " + ", ".join(missing))
        if unknown:
            detail.append("unknown " + ", ".join(unknown))
        raise RigError("configuration", f"{path}: {'; '.join(detail)}")


def parse_bool(value: str, field: str, path: pathlib.Path) -> bool:
    lowered = value.strip().lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    raise RigError("configuration", f"{path}: {field} must be true or false")


def resolve_commit(repo: pathlib.Path, requested_ref: str) -> str:
    try:
        result = subprocess.run(
            ("git", "-C", str(repo), "rev-parse", "--verify", f"{requested_ref}^{{commit}}"),
            check=True,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    except (OSError, subprocess.CalledProcessError) as error:
        raise RigError("checkout", f"cannot resolve requested ref {requested_ref}") from error
    return result.stdout.strip()


def make_run_id(commit_sha: str, mode: str, suffix: str = "") -> str:
    timestamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    tail = f"-{suffix}" if suffix else ""
    return f"{timestamp}-{commit_sha[:12]}-{mode}{tail}"


def artifact_hashes(directory: pathlib.Path) -> list[tuple[str, str]]:
    hashes = []
    excluded = {"run-manifest.txt", "run-manifest.txt.tmp"}
    for path in sorted(directory.rglob("*")):
        if path.is_symlink():
            raise RigError("manifest", f"artifact must not be a symlink: {path.relative_to(directory)}")
        if not path.is_file() or path.name in excluded:
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        hashes.append((path.relative_to(directory).as_posix(), digest))
    return hashes


def render_manifest(fields: Iterable[tuple[str, str]], hashes: Iterable[tuple[str, str]]) -> str:
    lines = ["manifest_version=1"]
    for key, value in fields:
        clean = value.replace("\r", " ").replace("\n", " ")
        lines.append(f"{key}={clean}")
    for path, digest in sorted(hashes):
        lines.append(f"artifact_sha256={digest}  {path}")
    return "\n".join(lines) + "\n"


def write_manifest(
    directory: pathlib.Path,
    result: RunResult,
    adapter_fields: Sequence[tuple[str, str]],
) -> None:
    fields = result.manifest_fields() + tuple(adapter_fields)
    keys = [key for key, _ in fields]
    duplicates = sorted({key for key in keys if keys.count(key) > 1})
    if duplicates:
        raise RigError("manifest", "duplicate manifest fields: " + ", ".join(duplicates))
    manifest = render_manifest(fields, artifact_hashes(directory))
    temporary = directory / "run-manifest.txt.tmp"
    temporary.write_text(manifest, encoding="utf-8")
    temporary.replace(directory / "run-manifest.txt")


def target_retained_value(target_state: str) -> str:
    if target_state == "not-created":
        return "not-created"
    if target_state == "retained":
        return "1"
    if target_state == "removed":
        return "0"
    raise RigError("manifest", f"unknown target state: {target_state}")


def cleanup_allowed(result: str, artifacts_collected: bool, manifest_finalized: bool) -> bool:
    return result == "passed" and artifacts_collected and manifest_finalized
