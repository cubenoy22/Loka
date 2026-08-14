#!/usr/bin/env python3
"""Assemble one release zip from an explicit, provenance-labelled file list."""

import argparse
import dataclasses
import hashlib
import pathlib
import subprocess
import sys
import tempfile
import zipfile


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
CANONICAL_CHECKOUT = pathlib.Path("/tmp/loka-release-assembly-worktree")
PACKAGE_SUFFIXES = {".lrp", ".lrpk"}


class ReleaseError(RuntimeError):
    pass


@dataclasses.dataclass(frozen=True)
class AllowlistEntry:
    kind: str
    source: str
    destination: str


def run_git(*arguments, cwd=REPOSITORY_ROOT, check=True):
    result = subprocess.run(
        ["git", *arguments],
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if check and result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "git command failed"
        raise ReleaseError(detail)
    return result


def clean_header_value(value):
    return str(value).replace("\r", " ").replace("\n", " ")


def sha256_bytes(payload):
    return hashlib.sha256(payload).hexdigest()


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def checked_relative_path(value, label):
    if not value or "\\" in value or "\0" in value:
        raise ReleaseError(f"invalid {label}: {value!r}")
    path = pathlib.PurePosixPath(value)
    invalid_part = any(part in ("", ".", "..") for part in path.parts)
    if path.is_absolute() or path.as_posix() != value or invalid_part:
        raise ReleaseError(f"invalid {label}: {value!r}")
    return value


def read_allowlist(path):
    try:
        payload = path.read_bytes()
        lines = payload.decode("utf-8").splitlines()
    except (OSError, UnicodeDecodeError) as error:
        raise ReleaseError(f"cannot read allowlist {path}: {error}") from error
    entries = []
    destinations = set()
    sources = set()
    for number, line in enumerate(lines, 1):
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) not in (2, 3) or fields[0] not in ("git", "build"):
            raise ReleaseError(
                f"{path}:{number}: expected kind<TAB>source[<TAB>archive-path], "
                "with kind git or build"
            )
        kind = fields[0]
        source = checked_relative_path(fields[1], "allowlist source")
        destination_value = fields[2] if len(fields) == 3 else source
        destination = checked_relative_path(destination_value, "archive path")
        if source in sources:
            raise ReleaseError(f"{path}:{number}: duplicate allowlist source: {source}")
        if destination in destinations:
            raise ReleaseError(f"{path}:{number}: duplicate archive path: {destination}")
        if kind != "git" and (
            pathlib.PurePosixPath(source).suffix.lower() in PACKAGE_SUFFIXES
            or pathlib.PurePosixPath(destination).suffix.lower() in PACKAGE_SUFFIXES
        ):
            raise ReleaseError(
                f"{path}:{number}: LRPK package must be git-tracked at the tag SHA: {source}"
            )
        sources.add(source)
        destinations.add(destination)
        entries.append(AllowlistEntry(kind, source, destination))
    if not entries:
        raise ReleaseError(f"allowlist has no files: {path}")
    return tuple(entries), sha256_bytes(payload)


def resolve_tag(tag):
    if not tag or "\n" in tag or "\r" in tag:
        raise ReleaseError("tag name must be a non-empty single line")
    reference = f"refs/tags/{tag}"
    tag_result = run_git("show-ref", "--verify", "--hash", reference, check=False)
    if tag_result.returncode != 0:
        raise ReleaseError(f"release tag does not exist locally: {tag}")
    commit_result = run_git("rev-parse", "--verify", f"{reference}^{{commit}}")
    return tag_result.stdout.strip(), commit_result.stdout.strip()


def path_is_within(path, parent):
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


def cleanup_worktree(checkout):
    removed = run_git("worktree", "remove", "--force", str(checkout), check=False)
    pruned = run_git("worktree", "prune", check=False)
    listed = run_git("worktree", "list", "--porcelain", check=False)
    registered = any(
        line == f"worktree {checkout}" for line in listed.stdout.splitlines()
    )
    checkout_exists = checkout.exists() or checkout.is_symlink()
    if not checkout_exists and listed.returncode == 0 and not registered:
        return
    details = []
    for result in (removed, pruned, listed):
        detail = result.stderr.strip() or result.stdout.strip()
        if result.returncode != 0 and detail:
            details.append(detail)
    if checkout_exists:
        details.append(f"checkout path remains: {checkout}")
    if registered:
        details.append(f"worktree registration remains: {checkout}")
    raise ReleaseError(
        "cannot remove canonical release checkout: "
        + ("; ".join(details) if details else "unknown error")
    )


def require_regular_source(checkout, relative):
    candidate = checkout.joinpath(*pathlib.PurePosixPath(relative).parts)
    current = checkout
    for part in pathlib.PurePosixPath(relative).parts:
        current = current / part
        if current.is_symlink():
            raise ReleaseError(f"listed file must not use a symlink: {relative}")
    if not candidate.is_file():
        raise ReleaseError(f"listed file is missing or is not a regular file: {relative}")
    try:
        candidate.resolve().relative_to(checkout.resolve())
    except ValueError as error:
        raise ReleaseError(f"listed file escapes the release checkout: {relative}") from error
    return candidate


def collect_files(checkout, entries, commit_sha):
    collected = []
    for entry in sorted(entries, key=lambda item: item.destination):
        source = require_regular_source(checkout, entry.source)
        if entry.kind == "git":
            tracked = run_git(
                "ls-files",
                "-z",
                "--error-unmatch",
                "--",
                entry.source,
                cwd=checkout,
                check=False,
            )
            if tracked.returncode != 0 or tracked.stdout != entry.source + "\0":
                raise ReleaseError(
                    f"listed git file is not tracked at {commit_sha}: {entry.source}"
                )
            unchanged = run_git(
                "diff",
                "--quiet",
                "HEAD",
                "--",
                entry.source,
                cwd=checkout,
                check=False,
            )
            if unchanged.returncode != 0:
                raise ReleaseError(f"listed git file differs from {commit_sha}: {entry.source}")
            provenance = f"git-tracked:{commit_sha}"
        else:
            provenance = f"build-output:{entry.source}"
        payload = source.read_bytes()
        collected.append((entry, payload, sha256_bytes(payload), provenance))
    return tuple(collected)


def write_archive(path, collected):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as archive:
        for entry, payload, _, _ in collected:
            info = zipfile.ZipInfo(entry.destination, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 3
            info.external_attr = 0o100644 << 16
            archive.writestr(info, payload)


def verify_archive(path, expected_hashes):
    try:
        with zipfile.ZipFile(path, "r") as archive:
            members = archive.infolist()
            names = [member.filename for member in members]
            if len(names) != len(set(names)):
                raise ReleaseError("archive contains duplicate member names")
            extra = sorted(set(names) - set(expected_hashes))
            missing = sorted(set(expected_hashes) - set(names))
            if extra:
                raise ReleaseError("unlisted archive member: " + ", ".join(extra))
            if missing:
                raise ReleaseError("listed archive member is missing: " + ", ".join(missing))
            for member in members:
                if member.is_dir():
                    raise ReleaseError(
                        f"archive contains unlisted directory entry: {member.filename}"
                    )
                actual = sha256_bytes(archive.read(member))
                if actual != expected_hashes[member.filename]:
                    raise ReleaseError(f"archive member hash mismatch: {member.filename}")
    except (OSError, zipfile.BadZipFile) as error:
        raise ReleaseError(f"cannot verify archive {path}: {error}") from error


def render_manifest(
    tag,
    tag_sha,
    commit_sha,
    archive_path,
    archive_name,
    allowlist_sha256,
    build_commands,
    collected,
):
    headers = (
        ("manifest_version", "1"),
        ("requested_tag", tag),
        ("tag_sha", tag_sha),
        ("commit_sha", commit_sha),
        ("checkout_path", CANONICAL_CHECKOUT.as_posix()),
        ("archive", archive_name),
        ("archive_sha256", sha256_file(archive_path)),
        ("allowlist_sha256", allowlist_sha256),
        ("build_command_count", str(len(build_commands))),
        ("lrpk_provenance_gate", "git-tracked-package-file-only"),
    )
    lines = [f"{key}={clean_header_value(value)}" for key, value in headers]
    for index, command in enumerate(build_commands, 1):
        lines.append(f"build_command_{index}={clean_header_value(command)}")
    for entry, _, _, provenance in collected:
        lines.append(f"provenance={provenance}  {entry.destination}")
    for entry, _, digest, _ in collected:
        lines.append(f"artifact_sha256={digest}  {entry.destination}")
    return "\n".join(lines) + "\n"


def assemble(tag, allowlist, archive, build_commands):
    entries, allowlist_sha256 = read_allowlist(allowlist)
    build_entries = tuple(entry for entry in entries if entry.kind == "build")
    tag_sha, commit_sha = resolve_tag(tag)
    checkout = CANONICAL_CHECKOUT
    archive = archive.resolve()
    manifest = pathlib.Path(str(archive) + ".manifest.txt")
    if checkout.exists():
        raise ReleaseError(f"canonical checkout path already exists: {checkout}")
    if path_is_within(archive, checkout) or path_is_within(manifest, checkout):
        raise ReleaseError("archive and manifest must be outside the canonical checkout")
    if archive.suffix.lower() != ".zip":
        raise ReleaseError("archive path must end in .zip")
    if archive.exists() or manifest.exists():
        raise ReleaseError("archive or manifest output already exists")
    for command in build_commands:
        if not command or "\n" in command or "\r" in command:
            raise ReleaseError("build commands must be non-empty single lines")
    if build_entries and not build_commands:
        raise ReleaseError("build entries require at least one build command")

    temporary_archive = None
    temporary_manifest = None
    try:
        try:
            run_git("worktree", "add", "--detach", str(checkout), commit_sha)
            actual_commit = run_git("rev-parse", "HEAD", cwd=checkout).stdout.strip()
            if actual_commit != commit_sha:
                raise ReleaseError(f"fresh checkout is at {actual_commit}, expected {commit_sha}")
            status = run_git(
                "status",
                "--porcelain",
                "--untracked-files=all",
                cwd=checkout,
            ).stdout
            if status:
                raise ReleaseError("fresh release checkout is not clean")
            for entry in build_entries:
                source = checkout.joinpath(*pathlib.PurePosixPath(entry.source).parts)
                if source.exists() or source.is_symlink():
                    raise ReleaseError(
                        f"listed build output exists before build commands: {entry.source}"
                    )
            for command in build_commands:
                result = subprocess.run(
                    ["/bin/sh", "-eu", "-c", command],
                    cwd=checkout,
                    check=False,
                )
                if result.returncode != 0:
                    raise ReleaseError(
                        f"build command failed with exit code {result.returncode}: {command}"
                    )
            collected = collect_files(checkout, entries, commit_sha)

            archive.parent.mkdir(parents=True, exist_ok=True)
            with tempfile.NamedTemporaryFile(
                prefix=archive.name + ".", suffix=".tmp", dir=archive.parent, delete=False
            ) as temporary:
                temporary_archive = pathlib.Path(temporary.name)
            write_archive(temporary_archive, collected)
            expected_hashes = {entry.destination: digest for entry, _, digest, _ in collected}
            verify_archive(temporary_archive, expected_hashes)
            with tempfile.NamedTemporaryFile(
                mode="w",
                encoding="utf-8",
                prefix=manifest.name + ".",
                suffix=".tmp",
                dir=archive.parent,
                delete=False,
            ) as temporary:
                temporary_manifest = pathlib.Path(temporary.name)
                temporary.write(
                    render_manifest(
                        tag,
                        tag_sha,
                        commit_sha,
                        temporary_archive,
                        archive.name,
                        allowlist_sha256,
                        build_commands,
                        collected,
                    )
                )
            temporary_archive.chmod(0o644)
            temporary_manifest.chmod(0o644)
        finally:
            cleanup_worktree(checkout)

        archive_published = False
        try:
            temporary_archive.replace(archive)
            temporary_archive = None
            archive_published = True
            temporary_manifest.replace(manifest)
            temporary_manifest = None
        except OSError:
            if archive_published:
                archive.unlink(missing_ok=True)
            raise
    finally:
        if temporary_archive is not None:
            temporary_archive.unlink(missing_ok=True)
        if temporary_manifest is not None:
            temporary_manifest.unlink(missing_ok=True)
    return archive, manifest


def parse_arguments(arguments):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True, help="local Git tag to assemble")
    parser.add_argument(
        "--allowlist",
        required=True,
        type=pathlib.Path,
        help="explicit release file list",
    )
    parser.add_argument("--archive", required=True, type=pathlib.Path, help="new .zip output path")
    parser.add_argument(
        "--build-command",
        action="append",
        default=[],
        help="shell command to run in the fresh checkout; may be repeated",
    )
    return parser.parse_args(arguments)


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        archive, manifest = assemble(
            options.tag,
            options.allowlist.resolve(),
            options.archive,
            tuple(options.build_command),
        )
    except (OSError, ReleaseError) as error:
        print(f"release assembly refused: {error}", file=sys.stderr)
        return 1
    print(archive)
    print(manifest)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
