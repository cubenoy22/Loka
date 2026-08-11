#!/usr/bin/env python3
"""Parse and compare one complete Loka SnapRecord."""

import argparse
import sys


REQUIRED_V1_KEYS = (
    "format_version",
    "schema_version",
    "scenario_version",
    "test",
    "step",
    "node",
    "tick",
    "status",
)


class SnapRecordError(ValueError):
    pass


def decode_value(value):
    decoded = []
    index = 0
    while index < len(value):
        character = value[index]
        if character != "\\":
            decoded.append(character)
            index += 1
            continue
        index += 1
        if index == len(value):
            raise SnapRecordError("dangling escape at end of value")
        escaped = value[index]
        if escaped == "\\":
            decoded.append("\\")
        elif escaped == "t":
            decoded.append("\t")
        elif escaped == "n":
            decoded.append("\n")
        else:
            raise SnapRecordError("unknown escape \\{}".format(escaped))
        index += 1
    return "".join(decoded)


def parse_record(path):
    with open(path, "r", encoding="utf-8", newline="") as handle:
        content = handle.read()
    if "\r" in content:
        raise SnapRecordError("record must use LF line endings")

    records = []
    current = []
    for line_number, line in enumerate(content.split("\n"), start=1):
        if line == "":
            if current:
                records.append(current)
                current = []
            continue
        current.append((line_number, line))
    if current:
        records.append(current)
    if len(records) != 1:
        raise SnapRecordError("expected exactly one record, found {}".format(len(records)))

    values = {}
    for line_number, line in records[0]:
        if "\t" not in line:
            raise SnapRecordError("line {} has no tab separator".format(line_number))
        key, encoded = line.split("\t", 1)
        if not key:
            raise SnapRecordError("line {} has an empty key".format(line_number))
        if key in values:
            raise SnapRecordError("line {} has duplicate key {!r}".format(line_number, key))
        values[key] = decode_value(encoded)

    for key in REQUIRED_V1_KEYS:
        if key not in values:
            raise SnapRecordError("record is missing required key {!r}".format(key))
    if values["format_version"] != "1":
        raise SnapRecordError("unsupported format_version {!r}".format(values["format_version"]))
    if values["status"] not in ("ok", "partial", "error"):
        raise SnapRecordError("invalid status {!r}".format(values["status"]))
    return values


def compare_records(expected_path, actual_path):
    expected = parse_record(expected_path)
    actual = parse_record(actual_path)
    differences = []
    for key in sorted(set(expected) - set(actual)):
        differences.append("missing key {} (expected {!r})".format(key, expected[key]))
    for key in sorted(set(actual) - set(expected)):
        differences.append("unexpected key {}={!r}".format(key, actual[key]))
    for key in sorted(set(expected) & set(actual)):
        if expected[key] != actual[key]:
            differences.append(
                "changed key {}: expected {!r}, actual {!r}".format(key, expected[key], actual[key])
            )
    if differences:
        for difference in differences:
            print("snaprecord: {}".format(difference), file=sys.stderr)
        return False
    return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    compare_parser = subparsers.add_parser("compare", help="compare every decoded key and value")
    compare_parser.add_argument("expected")
    compare_parser.add_argument("actual")

    get_parser = subparsers.add_parser("get", help="print one decoded value")
    get_parser.add_argument("record")
    get_parser.add_argument("key")
    arguments = parser.parse_args(argv)

    try:
        if arguments.command == "compare":
            return 0 if compare_records(arguments.expected, arguments.actual) else 1
        values = parse_record(arguments.record)
        if arguments.key not in values:
            raise SnapRecordError("record is missing key {!r}".format(arguments.key))
        print(values[arguments.key])
        return 0
    except (OSError, UnicodeError, SnapRecordError) as error:
        print("snaprecord: error: {}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
