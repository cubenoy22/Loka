#!/usr/bin/env python3
"""Record and validate the cell-local receipt used by --stage-last."""

import argparse
import pathlib
import sys

from classic_golden_identity import (
    IdentityError,
    UNATTESTABLE_SOURCE_IDENTITY,
    _read_strict_fields,
    _write_fields_atomic,
    sha256_file,
    source_tree_identity,
)


FIELDS = {
    "application_sha256", "source_tree_identity", "registry_sha256",
    "capture_adapter", "mode", "audit_verdict", "audit_sha256", "capture_sha256",
}


def run(args):
    if args.command == "audit-matched":
        fields = _read_strict_fields(args.provenance, FIELDS)
        fields["audit_verdict"] = "matched"
        fields["audit_sha256"] = sha256_file(args.audit)
        _write_fields_atomic(args.provenance, tuple(fields.items()))
        return
    if args.command == "capture-ready":
        # Bind the normalized capture to the receipt so --stage-last publishes
        # only the bytes this attested run produced.
        fields = _read_strict_fields(args.provenance, FIELDS)
        fields["capture_sha256"] = sha256_file(args.capture)
        _write_fields_atomic(args.provenance, tuple(fields.items()))
        return

    current = {
        "application_sha256": sha256_file(args.application),
        "source_tree_identity": source_tree_identity(args.source_tree),
        "registry_sha256": sha256_file(args.registry),
        "capture_adapter": args.capture_adapter,
        "mode": args.mode,
    }
    if args.command == "record":
        current.update(audit_verdict="not-matched", audit_sha256="none", capture_sha256="none")
        _write_fields_atomic(args.provenance, tuple(current.items()))
        return

    fields = _read_strict_fields(args.provenance, FIELDS)
    for key, value in current.items():
        if fields[key] != value:
            raise IdentityError(f"{key} differs: recorded={fields[key]!r}, current={value!r}")
    if current["source_tree_identity"] == UNATTESTABLE_SOURCE_IDENTITY:
        raise IdentityError("source_tree_identity is unattestable")
    if fields["audit_verdict"] != "matched":
        raise IdentityError("audit_verdict differs: the run did not match the tracked audit")
    for name in ("capture", "audit"):
        path = getattr(args, name)
        if not path.is_file() or path.is_symlink():
            raise IdentityError(f"missing finalized regular {name}: {path}")
    if fields["audit_sha256"] != sha256_file(args.audit):
        raise IdentityError("audit_sha256 differs from the matched run")
    if fields["audit_sha256"] != sha256_file(args.expected_audit):
        raise IdentityError("tracked audit_sha256 differs from the matched run")
    if fields["capture_sha256"] == "none":
        raise IdentityError("capture_sha256 is unset: the run did not normalize a capture")
    if fields["capture_sha256"] != sha256_file(args.capture):
        raise IdentityError("capture_sha256 differs from the attested run")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for command in ("record", "audit-matched", "capture-ready", "verify"):
        sub = commands.add_parser(command)
        sub.add_argument("--provenance", type=pathlib.Path, required=True)
        if command not in ("audit-matched", "capture-ready"):
            for name in ("application", "source-tree", "registry"):
                sub.add_argument("--" + name, type=pathlib.Path, required=True)
            sub.add_argument("--capture-adapter", required=True)
            sub.add_argument("--mode", choices=("capture", "probe", "structural-audit"), required=True)
        if command in ("audit-matched", "verify"):
            sub.add_argument("--audit", type=pathlib.Path, required=True)
        if command in ("capture-ready", "verify"):
            sub.add_argument("--capture", type=pathlib.Path, required=True)
        if command == "verify":
            sub.add_argument("--expected-audit", type=pathlib.Path, required=True)
    try:
        run(parser.parse_args())
    except (IdentityError, OSError, UnicodeError) as error:
        print(f"Scenario run provenance refused: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
