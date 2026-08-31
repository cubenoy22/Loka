#!/usr/bin/env python3
"""Report and gate final Retro68 68K MacBinary application sizes.

The checked-in baseline owns the shipping artifact paths and the material-growth
policy.  This tool reads each MacBinary resource fork directly so local and CI
runs measure the final file and the same CODE/DATA/RELA payloads.
"""

import argparse
import json
import pathlib
import struct
import sys


RESOURCE_TYPES = ("CODE", "DATA", "RELA")


class SizeReportError(Exception):
    pass


def read_u16(data, offset, context):
    if offset < 0 or offset + 2 > len(data):
        raise SizeReportError("%s is truncated" % context)
    return struct.unpack_from(">H", data, offset)[0]


def read_u24(data, offset, context):
    if offset < 0 or offset + 3 > len(data):
        raise SizeReportError("%s is truncated" % context)
    return (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]


def read_u32(data, offset, context):
    if offset < 0 or offset + 4 > len(data):
        raise SizeReportError("%s is truncated" % context)
    return struct.unpack_from(">I", data, offset)[0]


def padded_128(size):
    return (size + 127) & ~127


def resource_payload_sizes(path):
    data = path.read_bytes()
    if len(data) < 128:
        raise SizeReportError("%s: MacBinary header is truncated" % path)

    data_fork_length = read_u32(data, 83, "%s data-fork length" % path)
    resource_fork_length = read_u32(data, 87, "%s resource-fork length" % path)
    resource_start = 128 + padded_128(data_fork_length)
    resource_end = resource_start + resource_fork_length
    if resource_fork_length < 16 or resource_end > len(data):
        raise SizeReportError("%s: resource fork is missing or truncated" % path)

    fork = data[resource_start:resource_end]
    data_offset = read_u32(fork, 0, "%s resource data offset" % path)
    map_offset = read_u32(fork, 4, "%s resource map offset" % path)
    data_length = read_u32(fork, 8, "%s resource data length" % path)
    map_length = read_u32(fork, 12, "%s resource map length" % path)
    data_end = data_offset + data_length
    map_end = map_offset + map_length
    if data_offset < 16 or map_offset < 16:
        raise SizeReportError("%s: resource sections overlap the header" % path)
    if data_offset + data_length > len(fork):
        raise SizeReportError("%s: resource data section is out of bounds" % path)
    if map_offset + map_length > len(fork) or map_length < 28:
        raise SizeReportError("%s: resource map is out of bounds" % path)
    if not (data_end <= map_offset or map_end <= data_offset):
        raise SizeReportError("%s: resource data and map sections overlap" % path)

    type_list = map_offset + read_u16(
        fork, map_offset + 24, "%s type-list offset" % path
    )
    if type_list < map_offset or type_list + 2 > map_offset + map_length:
        raise SizeReportError("%s: resource type list is out of bounds" % path)

    encoded_type_count = read_u16(fork, type_list, "%s resource type count" % path)
    type_count = 0 if encoded_type_count == 0xFFFF else encoded_type_count + 1
    reference_floor = type_list + 2 + type_count * 8
    if reference_floor > map_end:
        raise SizeReportError("%s: resource type entries are out of bounds" % path)
    payloads = {}
    for type_index in range(type_count):
        entry = type_list + 2 + type_index * 8
        if entry + 8 > map_offset + map_length:
            raise SizeReportError("%s: resource type entry is out of bounds" % path)
        resource_type = fork[entry:entry + 4].decode("latin-1")
        encoded_resource_count = read_u16(
            fork, entry + 4, "%s %s resource count" % (path, resource_type)
        )
        resource_count = (
            0 if encoded_resource_count == 0xFFFF else encoded_resource_count + 1
        )
        references = type_list + read_u16(
            fork, entry + 6, "%s %s reference-list offset" % (path, resource_type)
        )
        if references < reference_floor or references + resource_count * 12 > map_end:
            raise SizeReportError(
                "%s: %s resource reference list is out of bounds"
                % (path, resource_type)
            )
        for resource_index in range(resource_count):
            reference = references + resource_index * 12
            payload_offset = read_u24(
                fork,
                reference + 5,
                "%s %s resource data offset" % (path, resource_type),
            )
            length_offset = data_offset + payload_offset
            payload_length = read_u32(
                fork,
                length_offset,
                "%s %s resource length" % (path, resource_type),
            )
            if length_offset + 4 + payload_length > data_offset + data_length:
                raise SizeReportError(
                    "%s: %s resource payload is out of bounds" % (path, resource_type)
                )
            payloads[resource_type] = payloads.get(resource_type, 0) + payload_length

    missing = [kind for kind in RESOURCE_TYPES if kind not in payloads]
    if missing:
        raise SizeReportError(
            "%s: required resource type(s) missing: %s" % (path, ", ".join(missing))
        )
    return {kind: payloads[kind] for kind in RESOURCE_TYPES}


def positive_integer(value, context, allow_zero=False):
    if isinstance(value, bool) or not isinstance(value, int):
        raise SizeReportError("%s must be an integer" % context)
    minimum = 0 if allow_zero else 1
    if value < minimum:
        raise SizeReportError("%s must be at least %d" % (context, minimum))
    return value


def load_baseline(path):
    try:
        baseline = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        raise SizeReportError("cannot read baseline %s: %s" % (path, error))

    if not isinstance(baseline, dict):
        raise SizeReportError("%s: baseline must be an object" % path)
    if baseline.get("schema_version") != 1:
        raise SizeReportError("%s: unsupported schema_version" % path)
    identity = baseline.get("identity")
    if not isinstance(identity, dict) or not identity:
        raise SizeReportError("%s: identity must be a non-empty object" % path)
    for key, value in identity.items():
        if not isinstance(key, str) or not key:
            raise SizeReportError("%s: identity keys must be non-empty strings" % path)
        if not isinstance(value, str) or not value:
            raise SizeReportError(
                "%s: identity values must be non-empty strings" % path
            )
    positive_integer(
        baseline.get("material_growth_bytes"),
        "%s material_growth_bytes" % path,
        allow_zero=True,
    )
    artifacts = baseline.get("artifacts")
    if not isinstance(artifacts, list) or not artifacts:
        raise SizeReportError("%s: artifacts must be a non-empty list" % path)

    names = set()
    paths = set()
    for index, artifact in enumerate(artifacts):
        context = "%s artifact %d" % (path, index + 1)
        if not isinstance(artifact, dict):
            raise SizeReportError("%s must be an object" % context)
        name = artifact.get("name")
        relative_path = artifact.get("path")
        if not isinstance(name, str) or not name:
            raise SizeReportError("%s name must be non-empty" % context)
        if not isinstance(relative_path, str) or not relative_path:
            raise SizeReportError("%s path must be non-empty" % context)
        candidate = pathlib.PurePosixPath(relative_path)
        if candidate.is_absolute() or ".." in candidate.parts:
            raise SizeReportError("%s path must stay under the build root" % context)
        if name in names or relative_path in paths:
            raise SizeReportError("%s duplicates an artifact name or path" % context)
        names.add(name)
        paths.add(relative_path)

        facts = artifact.get("baseline")
        if not isinstance(facts, dict):
            raise SizeReportError("%s baseline must be an object" % context)
        positive_integer(facts.get("total"), "%s baseline total" % context)
        for kind in RESOURCE_TYPES:
            positive_integer(
                facts.get(kind), "%s baseline %s" % (context, kind), allow_zero=True
            )
    return baseline


def signed(value):
    return "%+d" % value


def report(build_root, baseline):
    threshold = baseline["material_growth_bytes"]
    identity = baseline["identity"]
    declared_paths = set(artifact["path"] for artifact in baseline["artifacts"])
    example_root = build_root / "example"
    discovered_paths = set(
        path.relative_to(build_root).as_posix()
        for path in example_root.rglob("*68K.bin")
        if path.is_file()
    )
    unbaselined = sorted(discovered_paths - declared_paths)
    if unbaselined:
        raise SizeReportError(
            "unbaselined final 68K artifact(s): %s" % ", ".join(unbaselined)
        )

    identity_text = ", ".join(
        "%s=%s" % (key, identity[key]) for key in sorted(identity)
    )
    print("Retro68 68K final MacBinary size report")
    if identity_text:
        print("Baseline identity: %s" % identity_text)
    print("Material total-growth allowance: %d bytes" % threshold)
    print(
        "%-26s %9s %9s %9s %9s %9s %9s %9s %9s %s"
        % (
            "Artifact",
            "Total",
            "dTotal",
            "CODE",
            "dCODE",
            "DATA",
            "dDATA",
            "RELA",
            "dRELA",
            "Verdict",
        )
    )

    regressions = []
    for artifact in baseline["artifacts"]:
        path = build_root / pathlib.PurePosixPath(artifact["path"])
        if not path.is_file():
            raise SizeReportError("required artifact is missing: %s" % path)
        resources = resource_payload_sizes(path)
        current = {"total": path.stat().st_size}
        current.update(resources)
        facts = artifact["baseline"]
        deltas = {key: current[key] - facts[key] for key in current}
        material = deltas["total"] > threshold
        verdict = "REGRESSION" if material else "ok"
        if material:
            regressions.append((artifact["name"], deltas["total"]))
        print(
            "%-26s %9d %9s %9d %9s %9d %9s %9d %9s %s"
            % (
                artifact["name"],
                current["total"],
                signed(deltas["total"]),
                current["CODE"],
                signed(deltas["CODE"]),
                current["DATA"],
                signed(deltas["DATA"]),
                current["RELA"],
                signed(deltas["RELA"]),
                verdict,
            )
        )

    if regressions:
        print("Material Retro68 binary growth detected:", file=sys.stderr)
        for name, growth in regressions:
            print("  %s: +%d bytes" % (name, growth), file=sys.stderr)
        return 1
    return 0


def parse_arguments(arguments):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "build_root", type=pathlib.Path, help="Retro68 68K Release build directory"
    )
    parser.add_argument(
        "--baseline",
        type=pathlib.Path,
        default=pathlib.Path(__file__).with_name("retro68_68k_size_baseline.json"),
        help="checked-in size baseline manifest",
    )
    return parser.parse_args(arguments)


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        baseline = load_baseline(options.baseline)
        return report(options.build_root, baseline)
    except (OSError, SizeReportError) as error:
        print("retro68_size_report: %s" % error, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
