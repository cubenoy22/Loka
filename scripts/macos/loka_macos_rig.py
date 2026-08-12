#!/usr/bin/env python3
"""Run one specified Loka commit on a macOS Parallels rig.

This is the macOS vertical slice, not the future cross-platform loka-rig
orchestrator. Machine-specific names and paths come only from a local mapping.
"""

from __future__ import annotations

import argparse
import configparser
import dataclasses
import datetime
import hashlib
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import time
from typing import Iterable, Optional, Sequence


SUPPORTED_DESCRIPTOR_VERSION = "1"
SUPPORTED_ARTIFACT_CONTRACT_VERSION = "1"
SUPPORTED_MODES = frozenset(("flow", "inspect"))
RIG_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9.-]*$")


class RigError(RuntimeError):
    def __init__(self, stage: str, message: str):
        super().__init__(message)
        self.stage = stage


@dataclasses.dataclass(frozen=True)
class RigDescriptor:
    rig_id: str
    os_version: str
    os_build: str
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
    vm_host_ssh: str
    vm_name: str
    target_host: str
    target_user: str
    target_identity_file: pathlib.Path
    target_proxy_ssh: str
    target_legacy_rsa: bool
    target_root: pathlib.PurePosixPath
    archive_root: pathlib.Path
    vm_snapshot: str
    target_python: str


@dataclasses.dataclass(frozen=True)
class VmLease:
    initial_state: str

    def success_action(self) -> str:
        if self.initial_state == "stopped":
            return "stop"
        if self.initial_state == "suspended":
            return "suspend"
        return "leave-running"


def _read_single_section(path: pathlib.Path, section: str) -> configparser.SectionProxy:
    parser = configparser.ConfigParser(interpolation=None)
    try:
        with path.open("r", encoding="utf-8") as source:
            parser.read_file(source)
    except (OSError, configparser.Error) as error:
        raise RigError("configuration", f"cannot read {path}: {error}") from error
    if parser.sections() != [section]:
        raise RigError("configuration", f"{path} must contain only [{section}]")
    return parser[section]


def _require_keys(section: configparser.SectionProxy, expected: set[str], path: pathlib.Path) -> None:
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


def _parse_bool(value: str, field: str, path: pathlib.Path) -> bool:
    lowered = value.strip().lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    raise RigError("configuration", f"{path}: {field} must be true or false")


def load_descriptor(path: pathlib.Path) -> RigDescriptor:
    section = _read_single_section(path, "rig")
    expected = {
        "descriptor_version",
        "rig_id",
        "os_version",
        "os_build",
        "machine",
        "build_architecture",
        "build_profile",
        "supported_modes",
        "disposable_for_input",
        "capture_adapter",
        "recording_adapter",
        "artifact_contract_version",
    }
    _require_keys(section, expected, path)
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
    if section["build_profile"] != "macos-10.6-sdk-i386":
        raise RigError("configuration", f"{path}: unsupported build_profile")
    return RigDescriptor(
        rig_id=rig_id,
        os_version=section["os_version"].strip(),
        os_build=section["os_build"].strip(),
        machine=section["machine"].strip(),
        build_architecture=section["build_architecture"].strip(),
        build_profile=section["build_profile"].strip(),
        supported_modes=modes,
        disposable_for_input=_parse_bool(section["disposable_for_input"], "disposable_for_input", path),
        capture_adapter=section["capture_adapter"].strip(),
        recording_adapter=section["recording_adapter"].strip(),
        artifact_contract_version=section["artifact_contract_version"].strip(),
    )


def load_local_mapping(path: pathlib.Path) -> LocalMapping:
    section = _read_single_section(path, "local")
    required = {
        "vm_host_ssh",
        "vm_name",
        "target_host",
        "target_user",
        "target_identity_file",
        "target_proxy_ssh",
        "target_legacy_rsa",
        "target_root",
        "archive_root",
    }
    optional = {"vm_snapshot", "target_python"}
    actual = set(section.keys())
    missing = sorted(required - actual)
    unknown = sorted(actual - required - optional)
    if missing or unknown:
        detail = []
        if missing:
            detail.append("missing " + ", ".join(missing))
        if unknown:
            detail.append("unknown " + ", ".join(unknown))
        raise RigError("configuration", f"{path}: {'; '.join(detail)}")
    target_root = pathlib.PurePosixPath(section["target_root"].strip())
    archive_root = pathlib.Path(section["archive_root"].strip()).expanduser()
    if not target_root.is_absolute() or str(target_root) == "/":
        raise RigError("configuration", f"{path}: target_root must be a narrow absolute path")
    if not archive_root.is_absolute() or str(archive_root) == "/":
        raise RigError("configuration", f"{path}: archive_root must be a narrow absolute path")
    identity_file = pathlib.Path(section["target_identity_file"].strip()).expanduser()
    if not identity_file.is_absolute() or str(identity_file) == "/":
        raise RigError("configuration", f"{path}: target_identity_file must be a narrow absolute path")
    for field in ("vm_host_ssh", "vm_name", "target_host", "target_user"):
        if not section[field].strip() or "\n" in section[field] or "\r" in section[field]:
            raise RigError("configuration", f"{path}: invalid {field}")
    return LocalMapping(
        vm_host_ssh=section["vm_host_ssh"].strip(),
        vm_name=section["vm_name"].strip(),
        target_host=section["target_host"].strip(),
        target_user=section["target_user"].strip(),
        target_identity_file=identity_file,
        target_proxy_ssh=section["target_proxy_ssh"].strip(),
        target_legacy_rsa=_parse_bool(section["target_legacy_rsa"], "target_legacy_rsa", path),
        target_root=target_root,
        archive_root=archive_root,
        vm_snapshot=section.get("vm_snapshot", "").strip(),
        target_python=section.get("target_python", "/usr/bin/python3").strip(),
    )


def utc_now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def remote_command(arguments: Sequence[str]) -> str:
    return " ".join(shlex.quote(argument) for argument in arguments)


def remote_in_directory(directory: pathlib.PurePosixPath, arguments: Sequence[str], environment: dict[str, str] | None = None) -> str:
    assignments = []
    if environment:
        assignments = [f"{key}={shlex.quote(value)}" for key, value in sorted(environment.items())]
    environment_prefix = " ".join(assignments)
    if environment_prefix:
        environment_prefix += " "
    command = remote_command(tuple(arguments))
    return f"cd {shlex.quote(str(directory))} && {environment_prefix}exec {command}"


def artifact_hashes(directory: pathlib.Path) -> list[tuple[str, str]]:
    hashes = []
    for path in sorted(directory.rglob("*")):
        if not path.is_file() or path.name == "run-manifest.txt":
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


def cleanup_allowed(result: str, artifacts_collected: bool, manifest_finalized: bool) -> bool:
    return result == "passed" and artifacts_collected and manifest_finalized


def target_retained_value(target_state: str) -> str:
    if target_state == "not-created":
        return "not-created"
    if target_state == "retained":
        return "1"
    if target_state == "removed":
        return "0"
    raise RigError("manifest", f"unknown target state: {target_state}")


def parse_parallels_ipv4(output: str) -> Optional[str]:
    match = re.search(r"IP Addresses:\s+([0-9]+(?:\.[0-9]+){3})", output)
    return match.group(1) if match else None


def parse_parallels_state(output: str) -> Optional[str]:
    match = re.search(r"^State:\s+(running|suspended|stopped)\s*$", output, re.MULTILINE | re.IGNORECASE)
    return match.group(1).lower() if match else None


class CommandLog:
    def __init__(self, path: pathlib.Path):
        self._path = path

    def write(self, message: str) -> None:
        with self._path.open("a", encoding="utf-8") as output:
            output.write(f"[{utc_now()}] {message}\n")
        print(message, flush=True)


class MacOSRigRun:
    def __init__(
        self,
        repo: pathlib.Path,
        descriptor: RigDescriptor,
        mapping: LocalMapping,
        requested_ref: str,
        mode: str,
        scenario: str,
        release_after_ready: bool,
    ):
        self.repo = repo
        self.descriptor = descriptor
        self.mapping = mapping
        self.requested_ref = requested_ref
        self.mode = mode
        self.scenario = scenario
        self.release_after_ready = release_after_ready
        self.commit_sha = ""
        self.run_id = ""
        self.archive: Optional[pathlib.Path] = None
        self.checkout: Optional[pathlib.Path] = None
        self.target_run_root = pathlib.PurePosixPath("/")
        self.target_source = pathlib.PurePosixPath("/")
        self.target_artifacts = pathlib.PurePosixPath("/")
        self.target_destination = ""
        self.target_state = "not-created"
        self.vm_lease: Optional[VmLease] = None
        self.started_at = utc_now()
        self.ended_at = ""
        self.failure_stage = ""
        self.failure_message = ""
        self.artifacts_collected = False
        self.manifest_finalized = False
        self.build_passed = False
        self.runtime_passed = False
        self.command_log: Optional[CommandLog] = None

    def _run(self, args: Sequence[str], stage: str, **kwargs: object) -> subprocess.CompletedProcess[str]:
        try:
            return subprocess.run(args, check=True, text=True, **kwargs)
        except subprocess.CalledProcessError as error:
            raise RigError(stage, f"{args[0]} command exited {error.returncode}") from error
        except subprocess.TimeoutExpired as error:
            raise RigError(stage, f"{args[0]} command timed out") from error
        except OSError as error:
            raise RigError(stage, f"could not execute {args[0]}: {error}") from error

    def _ssh_alias(
        self, alias: str, arguments: Sequence[str], stage: str, capture: bool = True
    ) -> subprocess.CompletedProcess[str]:
        kwargs: dict[str, object] = {"timeout": 30}
        if capture:
            kwargs["stdout"] = subprocess.PIPE
            kwargs["stderr"] = subprocess.PIPE
        return self._run(
            ("ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", alias, remote_command(arguments)),
            stage,
            **kwargs,
        )

    def _discover_target(self) -> None:
        host = self.mapping.target_host
        if host == "auto":
            result = self._ssh_alias(
                self.mapping.vm_host_ssh,
                ("/usr/local/bin/prlctl", "list", "-i", self.mapping.vm_name),
                "target-discovery",
            )
            discovered = parse_parallels_ipv4(result.stdout or "")
            if not discovered:
                raise RigError("target-discovery", "Parallels did not report a target IPv4 address")
            host = discovered
        self.target_destination = f"{self.mapping.target_user}@{host}"

    def _target_transport_options(self, timeout: str) -> list[str]:
        options = [
            "-o",
            "BatchMode=yes",
            "-o",
            f"ConnectTimeout={timeout}",
            "-o",
            "StrictHostKeyChecking=accept-new",
        ]
        if self.mapping.target_legacy_rsa:
            options.extend(("-o", "HostKeyAlgorithms=+ssh-rsa", "-o", "PubkeyAcceptedAlgorithms=+ssh-rsa"))
        options.extend(("-o", "IdentitiesOnly=yes", "-i", str(self.mapping.target_identity_file)))
        if self.mapping.target_proxy_ssh:
            options.extend(("-J", self.mapping.target_proxy_ssh))
        return options

    def _target_ssh_args(self, timeout: str = "10") -> tuple[str, ...]:
        if not self.target_destination:
            raise RigError("target-preflight", "target endpoint was not discovered")
        return tuple(("ssh",) + tuple(self._target_transport_options(timeout)) + (self.target_destination,))

    def _target_remote_shell(self) -> str:
        return remote_command(("ssh",) + tuple(self._target_transport_options("10")))

    def _target_ssh(
        self, arguments: Sequence[str], stage: str, capture: bool = True
    ) -> subprocess.CompletedProcess[str]:
        kwargs: dict[str, object] = {"timeout": 30}
        if capture:
            kwargs["stdout"] = subprocess.PIPE
            kwargs["stderr"] = subprocess.PIPE
        return self._run(self._target_ssh_args() + (remote_command(arguments),), stage, **kwargs)

    def _rsync(self, arguments: Sequence[str], stage: str) -> None:
        last_error: Optional[RigError] = None
        for attempt in range(1, 4):
            try:
                self._run(("rsync", "--partial", "--timeout=30") + tuple(arguments), stage)
                return
            except RigError as error:
                last_error = error
                if self.command_log:
                    self.command_log.write(f"{stage} rsync attempt {attempt}/3 failed")
                if attempt < 3:
                    time.sleep(2)
        assert last_error is not None
        raise last_error

    def _vm_state(self) -> str:
        result = self._ssh_alias(
            self.mapping.vm_host_ssh,
            ("/usr/local/bin/prlctl", "list", "-i", self.mapping.vm_name),
            "vm-preflight",
        )
        state = parse_parallels_state(result.stdout or "")
        if not state:
            raise RigError("vm-preflight", "could not determine VM state")
        return state

    def _prepare_vm(self) -> None:
        initial = self._vm_state()
        self.vm_lease = VmLease(initial)
        self.command_log.write(f"VM initial state: {initial}")
        if initial in ("stopped", "suspended"):
            self._ssh_alias(
                self.mapping.vm_host_ssh,
                ("/usr/local/bin/prlctl", "start", self.mapping.vm_name),
                "vm-start",
            )
        self._discover_target()
        deadline = time.monotonic() + 120
        while True:
            attempt = subprocess.run(
                self._target_ssh_args("5") + ("true",),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            if attempt.returncode == 0:
                return
            if time.monotonic() >= deadline:
                raise RigError("target-preflight", "target SSH did not become ready")
            time.sleep(2)

    def _preflight_target(self) -> None:
        product = self._target_ssh(("/usr/bin/sw_vers", "-productVersion"), "target-preflight")
        build = self._target_ssh(("/usr/bin/sw_vers", "-buildVersion"), "target-preflight")
        machine = self._target_ssh(("/usr/bin/uname", "-m"), "target-preflight")
        facts = (product.stdout.strip(), build.stdout.strip(), machine.stdout.strip())
        expected = (self.descriptor.os_version, self.descriptor.os_build, self.descriptor.machine)
        if facts != expected:
            raise RigError("target-preflight", f"descriptor facts {expected} do not match target {facts}")
        console = self._target_ssh(
            ("/usr/bin/stat", "-f", "%Su", "/dev/console"),
            "aqua-preflight",
        ).stdout.strip()
        if console in ("", "root", "loginwindow"):
            raise RigError("aqua-preflight", "no active Aqua console user")
        self._target_ssh(("/usr/bin/pgrep", "-x", "WindowServer"), "aqua-preflight")
        required = (
            "/opt/local/bin/cmake",
            "/opt/local/bin/ninja",
            self.mapping.target_python,
            "/usr/sbin/screencapture",
            "/Developer/SDKs/MacOSX10.6.sdk",
        )
        for path in required:
            self._target_ssh(("/bin/test", "-e", path), "toolchain-preflight")

    def _prepare_checkout(self) -> None:
        result = self._run(
            ("git", "-C", str(self.repo), "rev-parse", "--verify", f"{self.requested_ref}^{{commit}}"),
            "checkout",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self.commit_sha = result.stdout.strip()
        timestamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
        self.run_id = f"{timestamp}-{self.commit_sha[:12]}-{self.mode}-{self.scenario}"
        self.archive = self.mapping.archive_root / self.descriptor.rig_id / self.run_id
        self.checkout = self.repo / "build" / "loka-rig" / "checkouts" / self.run_id
        self.target_run_root = self.mapping.target_root / self.run_id
        self.target_source = self.target_run_root / "source"
        self.target_artifacts = self.target_source / "build" / "loka-rig-run"
        self.archive.mkdir(parents=True, exist_ok=False)
        self.command_log = CommandLog(self.archive / "orchestrator.log")
        self.command_log.write(f"Resolved {self.requested_ref} to {self.commit_sha}")
        self.checkout.parent.mkdir(parents=True, exist_ok=True)
        self._run(
            ("git", "-C", str(self.repo), "worktree", "add", "--detach", str(self.checkout), self.commit_sha),
            "checkout",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def _transfer_source(self) -> None:
        self._target_ssh(
            ("/bin/mkdir", "-p", str(self.target_source)),
            "source-transfer",
        )
        self.target_state = "retained"
        self._rsync(
            (
                "--archive",
                "--delete",
                "-e",
                self._target_remote_shell(),
                "--exclude=.git",
                "--exclude=build",
                f"{self.checkout}/",
                f"{self.target_destination}:{self.target_source}/",
            ),
            "source-transfer",
        )

    def _run_logged_ssh(self, command: str, log_name: str, stage: str) -> None:
        assert self.archive is not None
        log_path = self.archive / log_name
        marker = self.target_run_root / f".{stage}-passed"
        wrapped = f"/bin/sh -c {shlex.quote(command)} && /usr/bin/touch {shlex.quote(str(marker))}"
        with log_path.open("ab") as output:
            process = subprocess.Popen(
                self._target_ssh_args() + (wrapped,),
                stdout=output,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 1800
                while not self._remote_file_exists(marker):
                    result = process.poll()
                    if result is not None:
                        time.sleep(1)
                        if self._remote_file_exists(marker):
                            break
                        raise RigError(stage, f"remote command exited {result}; see {log_path}")
                    if time.monotonic() >= deadline:
                        raise RigError(stage, f"remote command timed out; see {log_path}")
                    time.sleep(2)
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()

    def _build(self) -> None:
        build_dir = self.target_source / "build" / "loka-rig-macos"
        configure = remote_in_directory(
            self.target_source,
            (
                "/opt/local/bin/cmake",
                "-S",
                ".",
                "-B",
                str(build_dir),
                "-G",
                "Ninja",
                "-DTEST_BUILD=ON",
                "-DLOKA_WARNINGS_AS_ERRORS=ON",
                "-DCMAKE_BUILD_TYPE=Debug",
                "-DCMAKE_MAKE_PROGRAM=/opt/local/bin/ninja",
                "-DCMAKE_OSX_SYSROOT=/Developer/SDKs/MacOSX10.6.sdk",
                "-DCMAKE_OSX_DEPLOYMENT_TARGET=10.6",
                "-DCMAKE_OSX_ARCHITECTURES=i386",
            ),
            {"PATH": "/opt/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"},
        )
        build = remote_in_directory(
            self.target_source,
            (
                "/opt/local/bin/cmake",
                "--build",
                str(build_dir),
                "--target",
                "LokaScrapbookScenarioMacOS",
                "--parallel",
                "2",
            ),
            {"PATH": "/opt/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"},
        )
        self._run_logged_ssh(configure, "build.log", "configure")
        self._run_logged_ssh(build, "build.log", "build")
        self.build_passed = True

    def _runner_command(self) -> str:
        app = self.target_source / "build" / "loka-rig-macos" / "apple" / "macos" / "LokaScrapbookScenarioMacOS.app"
        flag = "--inspect" if self.mode == "inspect" else "--ci-structural"
        environment = {
            "LOKA_MACOS_SCENARIO_APP": str(app),
            "LOKA_MACOS_SCENARIO_CAPTURE_DESKTOP": "1",
            "LOKA_MACOS_SCENARIO_RETAIN_ON_FAILURE": "1",
            "LOKA_MACOS_SCENARIO_WORK": str(self.target_artifacts),
            "PATH": "/opt/local/bin:/usr/bin:/bin:/usr/sbin:/sbin",
            "PYTHON3": self.mapping.target_python,
        }
        return remote_in_directory(
            self.target_source,
            ("/bin/bash", "tests/macos/run-scenario.sh", self.scenario, flag),
            environment,
        )

    def _remote_file_exists(self, path: pathlib.PurePosixPath) -> bool:
        result = subprocess.run(
            self._target_ssh_args() + (remote_command(("/bin/test", "-f", str(path))),),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return result.returncode == 0

    def _publish_release(self) -> None:
        with tempfile.NamedTemporaryFile("w", encoding="ascii", delete=False) as output:
            output.write("release-inspection\n")
            temporary = pathlib.Path(output.name)
        remote_temporary = pathlib.PurePosixPath(str(self.target_artifacts / "release.tmp"))
        try:
            self._run(
                tuple(("scp", "-q") + tuple(self._target_transport_options("10")))
                + (str(temporary), f"{self.target_destination}:{remote_temporary}"),
                "inspect-release",
            )
            self._target_ssh(
                ("/bin/mv", str(remote_temporary), str(self.target_artifacts / "release")),
                "inspect-release",
            )
        finally:
            temporary.unlink(missing_ok=True)

    def _launch(self) -> None:
        assert self.archive is not None
        runner_log = (self.archive / "runner-command.log").open("wb")
        process = subprocess.Popen(
            self._target_ssh_args() + (self._runner_command(),),
            stdout=runner_log,
            stderr=subprocess.STDOUT,
        )
        try:
            deadline = time.monotonic() + 140
            if self.mode == "inspect":
                ready = self.target_artifacts / "ready"
                while not self._remote_file_exists(ready):
                    if process.poll() is not None:
                        raise RigError("inspect-ready", "scenario exited before publishing ready")
                    if time.monotonic() >= deadline:
                        raise RigError("inspect-ready", "timed out waiting for ready")
                    time.sleep(0.5)
                self.command_log.write("Inspect ready marker observed; app is held")
                if not self.release_after_ready:
                    input("Inspect the target, then press Enter to release it: ")
                self._publish_release()
            verified = self.target_artifacts / "verified"
            while not self._remote_file_exists(verified):
                if process.poll() is not None:
                    observed_after_exit = False
                    for _ in range(5):
                        time.sleep(1)
                        if self._remote_file_exists(verified):
                            observed_after_exit = True
                            break
                    if observed_after_exit:
                        break
                    raise RigError("runtime", "scenario command exited before runner verification")
                if time.monotonic() >= deadline:
                    raise RigError("runtime", "timed out waiting for runner verification")
                time.sleep(2)
            self.command_log.write("Atomic runner verification marker observed")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            runner_log.close()
        self.runtime_passed = True

    def _collect(self) -> None:
        assert self.archive is not None
        self._rsync(
            (
                "--archive",
                "-e",
                self._target_remote_shell(),
                f"{self.target_destination}:{self.target_artifacts}/",
                f"{self.archive}/",
            ),
            "artifact-collection",
        )
        required = {
            "actual.png",
            "actual.profile",
            "actual.snap",
            "build.log",
            "complete",
            "desktop-after.png",
            "desktop-before.png",
            "orchestrator.log",
            "runner-command.log",
            "runner.log",
            "verified",
        }
        if self.mode == "inspect":
            required.update(("ready", "release"))
        missing = sorted(name for name in required if not (self.archive / name).is_file())
        if missing:
            raise RigError("artifact-collection", "missing artifacts: " + ", ".join(missing))
        self.artifacts_collected = True

    def _write_manifest(self, result: str) -> None:
        vm_action = self.vm_lease.success_action() if self.vm_lease else "not-acquired"
        fields = (
            ("rig_id", self.descriptor.rig_id),
            ("descriptor_version", SUPPORTED_DESCRIPTOR_VERSION),
            ("artifact_contract_version", self.descriptor.artifact_contract_version),
            ("requested_ref", self.requested_ref),
            ("commit_sha", self.commit_sha),
            ("os_version", self.descriptor.os_version),
            ("os_build", self.descriptor.os_build),
            ("machine", self.descriptor.machine),
            ("build_architecture", self.descriptor.build_architecture),
            ("build_profile", self.descriptor.build_profile),
            ("vm_snapshot", self.mapping.vm_snapshot or "not-configured"),
            ("vm_initial_state", self.vm_lease.initial_state if self.vm_lease else "not-acquired"),
            ("vm_success_action", vm_action),
            ("mode", self.mode),
            ("scenario", self.scenario),
            ("capture_adapter", self.descriptor.capture_adapter),
            ("recording_adapter", self.descriptor.recording_adapter),
            ("started_at", self.started_at),
            ("ended_at", self.ended_at or utc_now()),
            ("result", result),
            ("failure_stage", self.failure_stage or "none"),
            ("failure_message", self.failure_message or "none"),
            ("build_verification", "passed" if self.build_passed else "failed-or-not-reached"),
            ("runtime_verification", "passed" if self.runtime_passed else "failed-or-not-reached"),
            ("machine_verdict", "passed" if result == "passed" else "failed"),
            ("recording_status", "not-requested"),
            ("target_retained", target_retained_value(self.target_state)),
            ("target_workdir", str(self.target_run_root)),
            ("next_diagnostic_command", "use configured target SSH mapping" if result != "passed" else "none"),
        )
        manifest = render_manifest(fields, artifact_hashes(self.archive))
        temporary = self.archive / "run-manifest.txt.tmp"
        temporary.write_text(manifest, encoding="utf-8")
        temporary.replace(self.archive / "run-manifest.txt")
        self.manifest_finalized = True

    def _best_effort_collect(self) -> None:
        if str(self.target_artifacts) == "/" or self.archive is None:
            return
        try:
            subprocess.run(
                (
                    "rsync",
                    "--partial",
                    "--timeout=15",
                    "--archive",
                    "-e",
                    self._target_remote_shell(),
                    f"{self.target_destination}:{self.target_artifacts}/",
                    f"{self.archive}/",
                ),
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=60,
            )
        except (OSError, subprocess.TimeoutExpired):
            pass

    def _cleanup_success(self) -> None:
        self._target_ssh(
            ("/bin/rm", "-rf", str(self.target_run_root)),
            "cleanup",
        )
        self.target_state = "removed"
        assert self.checkout is not None
        self._run(
            ("git", "-C", str(self.repo), "worktree", "remove", "--force", str(self.checkout)),
            "cleanup",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if not self.vm_lease:
            return
        action = self.vm_lease.success_action()
        if action == "stop":
            self._ssh_alias(
                self.mapping.vm_host_ssh,
                ("/usr/local/bin/prlctl", "stop", self.mapping.vm_name),
                "cleanup",
            )
        elif action == "suspend":
            self._ssh_alias(
                self.mapping.vm_host_ssh,
                ("/usr/local/bin/prlctl", "suspend", self.mapping.vm_name),
                "cleanup",
            )

    def execute(self) -> pathlib.Path:
        result = "failed"
        try:
            self._prepare_checkout()
            if self.mode not in self.descriptor.supported_modes:
                raise RigError("configuration", f"{self.descriptor.rig_id} does not support {self.mode}")
            self._prepare_vm()
            self._preflight_target()
            self._transfer_source()
            self._build()
            self._launch()
            self._collect()
            result = "passed"
        except RigError as error:
            self.failure_stage = error.stage
            self.failure_message = str(error)
            if self.command_log:
                self.command_log.write(f"FAILED at {error.stage}: {error}")
            self._best_effort_collect()
        except KeyboardInterrupt:
            self.failure_stage = "interrupted"
            self.failure_message = "run interrupted by operator"
            if self.command_log:
                self.command_log.write("FAILED at interrupted: run interrupted by operator")
            self._best_effort_collect()
        finally:
            self.ended_at = utc_now()
            if self.archive is not None:
                try:
                    self._write_manifest(result)
                except (OSError, RigError) as error:
                    self.failure_stage = "manifest"
                    self.failure_message = str(error)
                    result = "failed"
            if cleanup_allowed(result, self.artifacts_collected, self.manifest_finalized):
                try:
                    self._cleanup_success()
                except RigError as error:
                    self.failure_stage = error.stage
                    self.failure_message = str(error)
                    result = "failed"
                self.manifest_finalized = False
                try:
                    self._write_manifest(result)
                except (OSError, RigError) as error:
                    self.failure_stage = "manifest"
                    self.failure_message = str(error)
                    result = "failed"
        if result != "passed":
            raise RigError(self.failure_stage or "run", self.failure_message or "run failed")
        assert self.archive is not None
        return self.archive


def parse_args(arguments: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    run = subparsers.add_parser("run", help="build and run one specified commit")
    run.add_argument("rig_id")
    run.add_argument("--ref", required=True)
    run.add_argument("--mode", required=True, choices=sorted(SUPPORTED_MODES))
    run.add_argument("--scenario", default="startup", choices=("startup", "flip-forward-back"))
    run.add_argument("--local-config", type=pathlib.Path)
    run.add_argument("--release-after-ready", action="store_true", help="test-only non-interactive inspect release")
    return parser.parse_args(arguments)


def main(arguments: Sequence[str]) -> int:
    args = parse_args(arguments)
    script = pathlib.Path(__file__).resolve()
    repo = script.parents[2]
    descriptor_path = script.parent / "rigs" / f"{args.rig_id}.ini"
    local_path = args.local_config
    if local_path is None:
        configured = os.environ.get("LOKA_RIG_LOCAL_CONFIG")
        local_path = pathlib.Path(configured) if configured else pathlib.Path.home() / ".config" / "loka" / "rigs" / f"{args.rig_id}.ini"
    try:
        descriptor = load_descriptor(descriptor_path)
        mapping = load_local_mapping(local_path)
        run = MacOSRigRun(
            repo=repo,
            descriptor=descriptor,
            mapping=mapping,
            requested_ref=args.ref,
            mode=args.mode,
            scenario=args.scenario,
            release_after_ready=args.release_after_ready,
        )
        archive = run.execute()
    except RigError as error:
        print(f"{error.stage} stage failed: {error}", file=sys.stderr)
        return 1
    print(f"macOS rig passed: {archive}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
