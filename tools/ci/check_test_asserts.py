#!/usr/bin/env python3
"""Rejects load-bearing calls left inside a plain assert() in test sources.

assert compiles its whole expression out under NDEBUG, so a call placed inside
one stops happening in a Release build -- #264 found 695 of them, including 153
`assert(scene.flushInvalidation())` where the call *is* the flush. Tests use
LOKA_VERIFY for that; plain assert stays for pure comparisons.

Nothing in a build can catch the regression: a reintroduced side-effect assert
compiles fine and, under NDEBUG, usually just makes a test check less than it
claims. So this check reads the corpus instead of a hand-kept list: any call
already written inside a LOKA_VERIFY somewhere in the tree is known to carry
work, and must not appear inside a plain assert. A new load-bearing API earns
its place in the list the first time someone wraps it correctly.

Escape hatch for a genuine false positive: append `// loka-assert-ok: <reason>`
to the assert line.
"""

import os
import re
import sys

TEST_DIRS = ("tests",)
SOURCE_SUFFIXES = (".cpp", ".hpp", ".h", ".mm", ".m", ".inc")

# Names that read as accessors wherever they appear. A LOKA_VERIFY around one
# of these (`capture.get(...)` filling an out-parameter) must not turn every
# `assert(x.get() == 3)` in the tree into a failure.
ACCESSOR_NAMES = frozenset(
    [
        "get",
        "value",
        "size",
        "count",
        "empty",
        "data",
        "at",
        "front",
        "back",
        "begin",
        "end",
        "c_str",
        "length",
    ]
)

CALL_NAME = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*\(")
SUBJECT_SKIP = frozenset(
    [
        "if",
        "while",
        "for",
        "switch",
        "return",
        "sizeof",
        "static_cast",
        "reinterpret_cast",
        "const_cast",
        "assert",
        "LOKA_VERIFY",
    ]
)
VERIFY_LINE = re.compile(r"\bLOKA_VERIFY\s*\(")
ASSERT_LINE = re.compile(r"(^|[^A-Za-z0-9_])assert\s*\(")
ALLOW = re.compile(r"//\s*loka-assert-ok:")


def source_files(root):
    for directory in TEST_DIRS:
        base = os.path.join(root, directory)
        for current, _dirs, files in os.walk(base):
            for name in files:
                if name.endswith(SOURCE_SUFFIXES):
                    yield os.path.join(current, name)


def subject_call(line, macro):
    """The call the expression is *about*, not every name it mentions.

    `LOKA_VERIFY(std::fread(a, 1, sizeof(a), f) == sizeof(a))` is about `fread`;
    counting every name would put `sizeof` on the list and flag half the tree.
    """
    start = line.find(macro)
    if start < 0:
        return None
    rest = line[start + len(macro):]
    open_paren = rest.find("(")
    if open_paren < 0:
        return None
    expression = rest[open_paren + 1:]
    for name in CALL_NAME.findall(expression):
        if name not in SUBJECT_SKIP:
            return name
    return None


def main():
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    files = sorted(source_files(root))

    load_bearing = set()
    for path in files:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for line in handle:
                if VERIFY_LINE.search(line):
                    name = subject_call(line, "LOKA_VERIFY")
                    if name:
                        load_bearing.add(name)
    load_bearing -= ACCESSOR_NAMES
    load_bearing.discard("LOKA_VERIFY")

    if not load_bearing:
        print("check_test_asserts: no LOKA_VERIFY corpus found", file=sys.stderr)
        return 1

    findings = []
    for path in files:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            for number, line in enumerate(handle, 1):
                if not ASSERT_LINE.search(line) or ALLOW.search(line):
                    continue
                if VERIFY_LINE.search(line):
                    continue
                name = subject_call(line, "assert")
                if name and name in load_bearing:
                    findings.append(
                        (os.path.relpath(path, root), number, [name], line.strip())
                    )

    if findings:
        print("Load-bearing calls inside a plain assert (use LOKA_VERIFY):\n")
        for path, number, hits, line in findings:
            print("%s:%d: %s" % (path, number, ", ".join(hits)))
            print("    %s" % line)
        print(
            "\n%d finding(s). assert() is erased under NDEBUG, so these calls "
            "stop happening in a Release build.\n"
            "Wrap the expression in LOKA_VERIFY, or append "
            "`// loka-assert-ok: <reason>` if the call really is pure."
            % len(findings)
        )
        return 1

    print(
        "check_test_asserts: %d files, %d load-bearing call names, no findings"
        % (len(files), len(load_bearing))
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
