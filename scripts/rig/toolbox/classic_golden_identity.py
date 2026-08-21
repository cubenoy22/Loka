#!/usr/bin/env python3
"""Own the strict Classic pixel-golden bundle and reference identity contract."""

from __future__ import annotations

import argparse
import ast
import configparser
import hashlib
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Mapping, Sequence


MANIFEST_VERSION = "2"
IDENTITY_VERSION = "1"
UNAPPROVED_IDENTITY = "unapproved"
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
SCENARIO_PART_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
IDENTITY_FIELDS = (
    "gcc_version",
    "universal_interfaces_version",
    "retro68_identity_kind",
    "retro68_identity",
    "mame_executable_sha256",
    "mame_rom_identity_kind",
    "mame_rom_identity",
    "ram_size",
    "machine",
    "capture_adapter",
    "boot_hd_sha256",
)


class IdentityError(RuntimeError):
    pass


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _sha256_text(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def _read_strict_fields(path: pathlib.Path, expected: set[str]) -> dict[str, str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise IdentityError(f"cannot read {path}: {error}") from error
    fields: dict[str, str] = {}
    for number, line in enumerate(lines, 1):
        if not line or "=" not in line:
            raise IdentityError(f"{path}:{number}: malformed manifest field")
        key, value = line.split("=", 1)
        if not re.fullmatch(r"[a-z][a-z0-9_]*", key):
            raise IdentityError(f"{path}:{number}: invalid manifest field {key!r}")
        if key in fields:
            raise IdentityError(f"{path}:{number}: duplicate manifest field {key}")
        fields[key] = value
    missing = sorted(expected - set(fields))
    unknown = sorted(set(fields) - expected)
    if missing or unknown:
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if unknown:
            details.append("unknown " + ", ".join(unknown))
        raise IdentityError(f"{path}: strict manifest rejected: {'; '.join(details)}")
    return fields


def read_scenarios(registry: pathlib.Path) -> tuple[tuple[str, str], ...]:
    try:
        raw_lines = registry.read_text(encoding="utf-8").splitlines()
    except OSError as error:
        raise IdentityError(f"cannot read scenario registry {registry}: {error}") from error
    scenarios = []
    for number, line in enumerate(raw_lines, 1):
        parts = line.split()
        if (
            len(parts) != 2
            or not SCENARIO_PART_PATTERN.fullmatch(parts[0])
            or not SCENARIO_PART_PATTERN.fullmatch(parts[1])
        ):
            raise IdentityError(f"{registry}:{number}: invalid scenario registry entry")
        scenarios.append((parts[0], parts[1]))
    if not scenarios:
        raise IdentityError(f"scenario registry is empty: {registry}")
    if len(set(scenarios)) != len(scenarios):
        raise IdentityError(f"duplicate scenario registry entry: {registry}")
    return tuple(scenarios)


def _identity_payload(fields: Mapping[str, str]) -> str:
    return "".join(f"{key}={fields[key]}\n" for key in IDENTITY_FIELDS)


def identity_sha256(fields: Mapping[str, str]) -> str:
    return _sha256_text(_identity_payload(fields))


def _manifest_keys(scenario_count: int) -> set[str]:
    keys = {
        "manifest_version",
        "identity_sha256",
        "scenario_registry_sha256",
        "scenario_count",
        *IDENTITY_FIELDS,
    }
    for index in range(1, scenario_count + 1):
        keys.add(f"scenario_{index:04d}")
        keys.add(f"golden_sha256_{index:04d}")
    return keys


def _read_descriptor_identity(descriptor: pathlib.Path) -> tuple[str, str]:
    parser = configparser.ConfigParser(interpolation=None, strict=True)
    try:
        with descriptor.open("r", encoding="utf-8") as source:
            parser.read_file(source)
    except (OSError, configparser.Error) as error:
        raise IdentityError(f"cannot read tracked rig descriptor {descriptor}: {error}") from error
    if parser.sections() != ["rig"]:
        raise IdentityError(f"{descriptor} must contain only [rig]")
    section = parser["rig"]
    try:
        approved = section["reference_identity_sha256"].strip()
        capture_adapter = section["capture_adapter"].strip()
    except KeyError as error:
        raise IdentityError(f"{descriptor}: missing {error.args[0]}") from error
    if approved != UNAPPROVED_IDENTITY and not SHA256_PATTERN.fullmatch(approved):
        raise IdentityError(
            f"{descriptor}: reference_identity_sha256 must be a SHA-256 digest or {UNAPPROVED_IDENTITY}"
        )
    if not capture_adapter:
        raise IdentityError(f"{descriptor}: capture_adapter is empty")
    return approved, capture_adapter


def _read_current_identity(path: pathlib.Path) -> dict[str, str]:
    expected = {"identity_version", "identity_sha256", *IDENTITY_FIELDS}
    fields = _read_strict_fields(path, expected)
    if fields["identity_version"] != IDENTITY_VERSION:
        raise IdentityError(f"{path}: unsupported identity_version={fields['identity_version']}")
    identity = {key: fields[key] for key in IDENTITY_FIELDS}
    calculated = identity_sha256(identity)
    if fields["identity_sha256"] != calculated:
        raise IdentityError(f"{path}: identity_sha256 does not match its recorded fields")
    return identity


def _write_fields_atomic(path: pathlib.Path, fields: Sequence[tuple[str, str]]) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.parent.mkdir(parents=True, exist_ok=True)
    text = "".join(f"{key}={value}\n" for key, value in fields)
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(path)


def _decode_macro(value: str) -> str:
    value = value.strip()
    if value.startswith('"'):
        try:
            decoded = ast.literal_eval(value)
        except (SyntaxError, ValueError):
            return value
        return str(decoded)
    return value or "unavailable"


def _compiler_macros(compiler: pathlib.Path) -> dict[str, str]:
    commands = (
        (str(compiler), "-dM", "-E", "-x", "c++", "-include", "ConditionalMacros.h", "-"),
        (str(compiler), "-dM", "-E", "-x", "c++", "-"),
    )
    for command in commands:
        try:
            result = subprocess.run(
                command,
                input=b"",
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                timeout=120,
            )
        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            continue
        macros: dict[str, str] = {}
        for raw_line in result.stdout.decode("utf-8", errors="replace").splitlines():
            match = re.match(r"^#define\s+(\S+)\s*(.*)$", raw_line)
            if match:
                macros[match.group(1)] = match.group(2)
        if "__VERSION__" in macros:
            return macros
    return {}


def _infer_toolchain_root(compiler: pathlib.Path, configured: str) -> pathlib.Path | None:
    if configured:
        candidate = pathlib.Path(configured)
        if candidate.is_dir():
            return candidate.resolve()
    try:
        resolved = compiler.resolve(strict=True)
    except OSError:
        return None
    for parent in resolved.parents:
        if parent.name == "toolchain":
            return parent
    return resolved.parent.parent if resolved.parent.parent.is_dir() else None


def _tree_content_sha256(root: pathlib.Path) -> str:
    digest = hashlib.sha256()
    found = False
    try:
        paths = sorted(root.rglob("*"), key=lambda path: path.relative_to(root).as_posix())
        for path in paths:
            relative = path.relative_to(root).as_posix().encode("utf-8")
            if path.is_symlink():
                digest.update(b"L\0" + relative + b"\0" + os.readlink(path).encode("utf-8") + b"\0")
                found = True
            elif path.is_file():
                digest.update(b"F\0" + relative + b"\0")
                with path.open("rb") as source:
                    for block in iter(lambda: source.read(1024 * 1024), b""):
                        digest.update(block)
                digest.update(b"\0")
                found = True
    except OSError as error:
        raise IdentityError(f"cannot digest Retro68 toolchain tree {root}: {error}") from error
    if not found:
        raise IdentityError(f"Retro68 toolchain tree contains no attestable content: {root}")
    return digest.hexdigest()


def emit_build_provenance(
    output: pathlib.Path, compiler: pathlib.Path, toolchain_root: str
) -> None:
    macros = _compiler_macros(compiler)
    gcc_version = _decode_macro(macros.get("__VERSION__", "unavailable"))
    interfaces_version = _decode_macro(
        macros.get("UNIVERSAL_INTERFACES_VERSION", "unavailable")
    )
    root = _infer_toolchain_root(compiler, toolchain_root)
    retro_kind = "unattestable"
    retro_identity = "unavailable"
    if root is not None:
        try:
            retro_identity = _tree_content_sha256(root)
            retro_kind = "toolchain-content-sha256"
        except IdentityError:
            pass
    _write_fields_atomic(
        output,
        (
            ("build_provenance_version", "1"),
            ("gcc_version", gcc_version),
            ("universal_interfaces_version", interfaces_version),
            ("retro68_identity_kind", retro_kind),
            ("retro68_identity", retro_identity),
        ),
    )


def _read_build_provenance(path: pathlib.Path) -> dict[str, str]:
    expected = {
        "build_provenance_version",
        "gcc_version",
        "universal_interfaces_version",
        "retro68_identity_kind",
        "retro68_identity",
    }
    fields = _read_strict_fields(path, expected)
    if fields["build_provenance_version"] != "1":
        raise IdentityError(f"{path}: unsupported build_provenance_version")
    if fields["retro68_identity_kind"] not in (
        "toolchain-content-sha256",
        "unattestable",
    ):
        raise IdentityError(f"{path}: unknown retro68_identity_kind")
    if fields["retro68_identity_kind"] == "toolchain-content-sha256":
        if not SHA256_PATTERN.fullmatch(fields["retro68_identity"]):
            raise IdentityError(f"{path}: invalid Retro68 toolchain digest")
    elif fields["retro68_identity"] != "unavailable":
        raise IdentityError(f"{path}: unattestable Retro68 identity must be unavailable")
    return fields


def _verified_rom_identity(
    executable: pathlib.Path, machine: str, rompath: str
) -> tuple[str, str]:
    prefix = [str(executable)]
    if rompath:
        prefix.extend(("-rompath", rompath))
    try:
        subprocess.run(
            prefix + ["-verifyroms", machine],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=180,
        )
        inventory = subprocess.run(
            prefix + ["-listroms", machine],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=180,
        ).stdout.replace(b"\r\n", b"\n")
    except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
        return "unattestable", "unavailable"
    if not inventory.strip():
        return "unattestable", "unavailable"
    return "verified-rom-inventory-sha256", hashlib.sha256(inventory).hexdigest()


def capture_current_identity(args: argparse.Namespace) -> None:
    provenance = _read_build_provenance(args.build_provenance)
    approved, descriptor_adapter = _read_descriptor_identity(args.descriptor)
    del approved
    if args.capture_adapter != descriptor_adapter:
        raise IdentityError(
            f"capture adapter {args.capture_adapter} does not match tracked {descriptor_adapter}"
        )
    if not args.mame_executable.is_file():
        raise IdentityError(f"MAME executable is not a regular file: {args.mame_executable}")
    if not args.boot_hd.is_file():
        raise IdentityError(f"copied Boot.hd is not a regular file: {args.boot_hd}")
    rom_kind, rom_identity = _verified_rom_identity(
        args.mame_executable, args.machine, args.rompath
    )
    identity = {
        "gcc_version": provenance["gcc_version"],
        "universal_interfaces_version": provenance["universal_interfaces_version"],
        "retro68_identity_kind": provenance["retro68_identity_kind"],
        "retro68_identity": provenance["retro68_identity"],
        "mame_executable_sha256": sha256_file(args.mame_executable),
        "mame_rom_identity_kind": rom_kind,
        "mame_rom_identity": rom_identity,
        "ram_size": args.ram_size,
        "machine": args.machine,
        "capture_adapter": args.capture_adapter,
        "boot_hd_sha256": sha256_file(args.boot_hd),
    }
    _write_fields_atomic(
        args.output,
        (
            ("identity_version", IDENTITY_VERSION),
            *((key, identity[key]) for key in IDENTITY_FIELDS),
            ("identity_sha256", identity_sha256(identity)),
        ),
    )


def _validate_manifest(
    bundle: pathlib.Path,
    registry: pathlib.Path,
    descriptor: pathlib.Path,
    *,
    current_identity: Mapping[str, str] | None = None,
    require_authorized: bool = True,
) -> tuple[tuple[str, str], ...]:
    scenarios = read_scenarios(registry)
    manifest = bundle / "manifest.txt"
    if not manifest.is_file():
        legacy = next(bundle.rglob("*.png.mame-machine"), None) if bundle.is_dir() else None
        detail = "legacy PNG/.mame-machine baseline" if legacy else "old-shape or absent baseline"
        raise IdentityError(
            f"{detail} at {bundle}; re-bake the complete Classic golden bundle with --update-golden"
        )
    fields = _read_strict_fields(manifest, _manifest_keys(len(scenarios)))
    if fields["manifest_version"] != MANIFEST_VERSION:
        raise IdentityError(
            f"{manifest}: unsupported manifest_version={fields['manifest_version']}; re-bake required"
        )
    if fields["scenario_count"] != str(len(scenarios)):
        raise IdentityError(f"{manifest}: scenario_count does not match the registry")
    registry_digest = sha256_file(registry)
    if fields["scenario_registry_sha256"] != registry_digest:
        raise IdentityError(f"{manifest}: scenario registry digest mismatch; re-bake required")
    recorded_identity = {key: fields[key] for key in IDENTITY_FIELDS}
    if recorded_identity["retro68_identity_kind"] not in (
        "toolchain-content-sha256",
        "unattestable",
    ):
        raise IdentityError(f"{manifest}: unknown retro68_identity_kind")
    if recorded_identity["retro68_identity_kind"] == "toolchain-content-sha256":
        if not SHA256_PATTERN.fullmatch(recorded_identity["retro68_identity"]):
            raise IdentityError(f"{manifest}: invalid Retro68 toolchain digest")
    elif recorded_identity["retro68_identity"] != "unavailable":
        raise IdentityError(f"{manifest}: unattestable Retro68 identity must be unavailable")
    if recorded_identity["mame_rom_identity_kind"] not in (
        "verified-rom-inventory-sha256",
        "unattestable",
    ):
        raise IdentityError(f"{manifest}: unknown mame_rom_identity_kind")
    if recorded_identity["mame_rom_identity_kind"] == "verified-rom-inventory-sha256":
        if not SHA256_PATTERN.fullmatch(recorded_identity["mame_rom_identity"]):
            raise IdentityError(f"{manifest}: invalid verified MAME ROM inventory digest")
    elif recorded_identity["mame_rom_identity"] != "unavailable":
        raise IdentityError(f"{manifest}: unattestable MAME ROM identity must be unavailable")
    for key in ("mame_executable_sha256", "boot_hd_sha256"):
        if not SHA256_PATTERN.fullmatch(recorded_identity[key]):
            raise IdentityError(f"{manifest}: invalid {key}")
    calculated_identity = identity_sha256(recorded_identity)
    if fields["identity_sha256"] != calculated_identity:
        raise IdentityError(f"{manifest}: identity_sha256 does not match every recorded identity field")
    if require_authorized and recorded_identity["gcc_version"] == "unavailable":
        raise IdentityError("GCC __VERSION__ is unattestable; pixel reference eligibility refused")
    if (
        require_authorized
        and recorded_identity["universal_interfaces_version"] == "unavailable"
    ):
        raise IdentityError(
            "UNIVERSAL_INTERFACES_VERSION is unattestable; pixel reference eligibility refused"
        )
    if require_authorized and recorded_identity["retro68_identity_kind"] == "unattestable":
        raise IdentityError("Retro68 identity is unattestable; pixel reference eligibility refused")
    if require_authorized and recorded_identity["mame_rom_identity_kind"] == "unattestable":
        raise IdentityError("resolved MAME ROM identity is unattestable; pixel reference eligibility refused")
    approved, descriptor_adapter = _read_descriptor_identity(descriptor)
    if recorded_identity["capture_adapter"] != descriptor_adapter:
        raise IdentityError(f"{manifest}: capture_adapter does not match the tracked rig descriptor")
    if require_authorized:
        if approved == UNAPPROVED_IDENTITY:
            raise IdentityError(
                "tracked reference identity is unapproved; approve the 0.0.3a re-bake digest in the rig descriptor"
            )
        if fields["identity_sha256"] != approved:
            raise IdentityError(
                f"bundle identity {fields['identity_sha256']} does not match tracked approved identity {approved}; "
                "a fresh --update-golden bake cannot self-authorize"
            )
    if current_identity is not None:
        for key in IDENTITY_FIELDS:
            if fields[key] != current_identity[key]:
                raise IdentityError(
                    f"identity mismatch for {key}: bundle={fields[key]!r}, current={current_identity[key]!r}"
                )
    for index, (example, scenario) in enumerate(scenarios, 1):
        relative = f"{example}/{scenario}.png"
        if fields[f"scenario_{index:04d}"] != relative:
            raise IdentityError(f"{manifest}: scenario_{index:04d} does not match the registry")
        golden = bundle / relative
        if not golden.is_file() or golden.is_symlink():
            raise IdentityError(f"missing finalized regular golden: {golden}")
        digest = sha256_file(golden)
        if fields[f"golden_sha256_{index:04d}"] != digest:
            raise IdentityError(f"golden digest mismatch: {golden}")
    expected_files = {"manifest.txt"} | {
        f"{example}/{scenario}.png" for example, scenario in scenarios
    }
    actual_files = set()
    for path in bundle.rglob("*"):
        if path.is_symlink():
            raise IdentityError(f"golden bundle entry must not be a symlink: {path}")
        if path.is_file():
            actual_files.add(path.relative_to(bundle).as_posix())
    unknown_files = sorted(actual_files - expected_files)
    if unknown_files:
        raise IdentityError("golden bundle contains unknown files: " + ", ".join(unknown_files))
    return scenarios


def validate_bundle(
    bundle: pathlib.Path, registry: pathlib.Path, descriptor: pathlib.Path
) -> tuple[tuple[str, str], ...]:
    return _validate_manifest(bundle, registry, descriptor, require_authorized=True)


def verify_bundle(args: argparse.Namespace) -> None:
    current = _read_current_identity(args.current_identity)
    scenarios = _validate_manifest(
        args.bundle,
        args.registry,
        args.descriptor,
        current_identity=current,
        require_authorized=True,
    )
    if (args.example, args.scenario) not in scenarios:
        raise IdentityError(f"scenario is not registered: {args.example} {args.scenario}")


def _render_bundle_manifest(
    staging: pathlib.Path,
    registry: pathlib.Path,
    scenarios: Sequence[tuple[str, str]],
    identity: Mapping[str, str],
) -> str:
    lines = [f"manifest_version={MANIFEST_VERSION}"]
    lines.extend(f"{key}={identity[key]}" for key in IDENTITY_FIELDS)
    lines.append(f"identity_sha256={identity_sha256(identity)}")
    lines.append(f"scenario_registry_sha256={sha256_file(registry)}")
    lines.append(f"scenario_count={len(scenarios)}")
    for index, (example, scenario) in enumerate(scenarios, 1):
        relative = f"{example}/{scenario}.png"
        lines.append(f"scenario_{index:04d}={relative}")
        lines.append(f"golden_sha256_{index:04d}={sha256_file(staging / relative)}")
    return "\n".join(lines) + "\n"


def _publish_bundle(staging: pathlib.Path, bundle: pathlib.Path) -> None:
    backup = bundle.with_name(bundle.name + ".previous")
    if backup.exists():
        raise IdentityError(f"stale golden bundle backup blocks publication: {backup}")
    moved_old = False
    try:
        if bundle.exists():
            bundle.replace(backup)
            moved_old = True
        staging.replace(bundle)
    except OSError as error:
        if moved_old and not bundle.exists():
            try:
                backup.replace(bundle)
            except OSError:
                pass
        raise IdentityError(f"could not atomically publish golden bundle: {error}") from error
    if moved_old:
        try:
            shutil.rmtree(backup)
        except OSError as error:
            # Publication is already committed. Report cleanup trouble without
            # mislabeling the complete new bundle as a failed/partial bake.
            print(f"Warning: could not remove prior golden bundle {backup}: {error}", file=sys.stderr)


def stage_capture(args: argparse.Namespace) -> bool:
    scenarios = read_scenarios(args.registry)
    if (args.example, args.scenario) not in scenarios:
        raise IdentityError(f"scenario is not registered: {args.example} {args.scenario}")
    if not args.capture.is_file() or args.capture.is_symlink():
        raise IdentityError(f"capture is not a finalized regular file: {args.capture}")
    identity = _read_current_identity(args.current_identity)
    staging = args.bundle.with_name(args.bundle.name + ".incomplete")
    identity_partial = staging / "identity.partial"
    if staging.exists() and not staging.is_dir():
        raise IdentityError(f"golden bake staging path is not a directory: {staging}")
    if staging.is_dir() and identity_partial.is_file():
        staged_identity = _read_current_identity(identity_partial)
        for key in IDENTITY_FIELDS:
            if staged_identity[key] != identity[key]:
                raise IdentityError(
                    f"incomplete bake identity differs for {key}; remove {staging} and restart the bake"
                )
    elif staging.exists():
        raise IdentityError(f"incomplete bake has no strict identity: {staging}")
    else:
        staging.mkdir(parents=True)
        shutil.copy2(args.current_identity, identity_partial)
    destination = staging / args.example / f"{args.scenario}.png"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary_capture = destination.with_name(destination.name + ".tmp")
    shutil.copy2(args.capture, temporary_capture)
    temporary_capture.replace(destination)
    missing = [
        f"{example}/{scenario}.png"
        for example, scenario in scenarios
        if not (staging / example / f"{scenario}.png").is_file()
    ]
    if missing:
        print(
            f"Staged Classic golden {args.example}/{args.scenario}; "
            f"{len(missing)} registered capture(s) remain before publication"
        )
        return False
    identity_partial.unlink()
    manifest = staging / "manifest.txt"
    _write_fields_atomic(
        manifest,
        tuple(
            line.split("=", 1)
            for line in _render_bundle_manifest(staging, args.registry, scenarios, identity).splitlines()
        ),
    )
    _validate_manifest(
        staging,
        args.registry,
        args.descriptor,
        current_identity=identity,
        require_authorized=False,
    )
    _publish_bundle(staging, args.bundle)
    approved, _ = _read_descriptor_identity(args.descriptor)
    digest = identity_sha256(identity)
    print(f"Published complete Classic golden bundle: {args.bundle}")
    if approved == digest:
        print(f"Reference eligibility: eligible (tracked identity {digest})")
    else:
        print(
            f"Reference eligibility: ineligible; bundle identity {digest} is not the tracked approved identity "
            f"{approved}. A bake cannot self-authorize."
        )
    return True


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    emit = subparsers.add_parser("emit-build-provenance")
    emit.add_argument("--output", required=True, type=pathlib.Path)
    emit.add_argument("--compiler", required=True, type=pathlib.Path)
    emit.add_argument("--toolchain-root", default="")

    current = subparsers.add_parser("capture-current")
    current.add_argument("--output", required=True, type=pathlib.Path)
    current.add_argument("--build-provenance", required=True, type=pathlib.Path)
    current.add_argument("--descriptor", required=True, type=pathlib.Path)
    current.add_argument("--mame-executable", required=True, type=pathlib.Path)
    current.add_argument("--rompath", default="")
    current.add_argument("--ram-size", required=True)
    current.add_argument("--machine", required=True)
    current.add_argument("--capture-adapter", required=True)
    current.add_argument("--boot-hd", required=True, type=pathlib.Path)

    verify = subparsers.add_parser("verify")
    verify.add_argument("--bundle", required=True, type=pathlib.Path)
    verify.add_argument("--registry", required=True, type=pathlib.Path)
    verify.add_argument("--descriptor", required=True, type=pathlib.Path)
    verify.add_argument("--current-identity", required=True, type=pathlib.Path)
    verify.add_argument("--example", required=True)
    verify.add_argument("--scenario", required=True)

    stage = subparsers.add_parser("stage-capture")
    stage.add_argument("--bundle", required=True, type=pathlib.Path)
    stage.add_argument("--registry", required=True, type=pathlib.Path)
    stage.add_argument("--descriptor", required=True, type=pathlib.Path)
    stage.add_argument("--current-identity", required=True, type=pathlib.Path)
    stage.add_argument("--capture", required=True, type=pathlib.Path)
    stage.add_argument("--example", required=True)
    stage.add_argument("--scenario", required=True)
    return parser


def main(arguments: Sequence[str]) -> int:
    args = _build_parser().parse_args(arguments)
    try:
        if args.command == "emit-build-provenance":
            emit_build_provenance(args.output, args.compiler, args.toolchain_root)
        elif args.command == "capture-current":
            capture_current_identity(args)
        elif args.command == "verify":
            verify_bundle(args)
        elif args.command == "stage-capture":
            stage_capture(args)
        else:
            raise AssertionError(args.command)
    except (IdentityError, OSError) as error:
        print(f"Classic golden identity refused: {error}", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
