"""Probe RetroPPC's generated stack layout using the actual QuickJS compiler.

This is a compiler-layout pin, not a replacement for running Scene switches
in MAME. The ordinary-alloca control reproduces the GCC 16.1 collision.
"""
import argparse
import json
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


PROBE = r"""
#include "ClassicQuickjsStack.h"
extern void consume(void *, void *, int, int, int, int, int, int, int, int, int);
__attribute__((noinline)) int probe(unsigned size) {
    volatile unsigned guard[12];
    unsigned i;
    unsigned char *p = SMIRKYCARD_JS_ALLOCA(size);
    for (i = 0; i < 12; ++i) guard[i] = 0x12345678;
    for (i = 0; i < size; ++i) p[i] = 0xAA;
    consume(p, (void *)guard, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    for (i = 0; i < 12; ++i) if (guard[i] != 0x12345678) return 0;
    return 1;
}
"""


def offsets(assembly):
    # The pinned -Os probe passes p in r3 and guard in r4, retaining the
    # pre-alloca frame in r31. Refuse an unknown compiler shape instead of
    # silently concluding that the arrays are disjoint.
    frame = re.search(r"\bmr\s+31,1\b", assembly)
    dynamic = re.search(r"\baddi\s+3,1,(\d+)\b", assembly)
    rounding = re.search(r"\baddi\s+\d+,3,(\d+)\b", assembly)
    guard_reg = re.search(r"\bmr\s+4,(\d+)\b", assembly)
    guard = (re.search(r"\baddi\s+" + guard_reg[1] + r",31,(\d+)\b", assembly)
             if guard_reg else None)
    if not (frame and dynamic and guard and rounding):
        raise AssertionError("Compiler layout changed; inspect the probe assembly")
    # GCC rounds size + padding upward to 16. With a multiple-of-16 payload,
    # the aligned part of the addend is the extra stack space reserved.
    padding = int(rounding[1]) & ~15
    return int(dynamic[1]) - padding, int(guard[1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()
    entries = json.loads((args.build_dir / "compile_commands.json").read_text())
    entry = next(e for e in entries if e["file"].endswith("/quickjs.c"))
    original = entry.get("arguments") or shlex.split(entry["command"])
    triple = subprocess.check_output([original[0], "-dumpmachine"], text=True).strip()
    if triple != "powerpc-apple-macos":
        print("[skip] RetroPPC compiler-layout pin: target is " + triple)
        return
    prepared = Path(entry["file"]).read_text()
    assert prepared.count("SMIRKYCARD_JS_ALLOCA(") == 5, "Missing allocation-site wiring"
    assert "sp = js_get_stack_pointer() - SMIRKYCARD_JS_STACK_SIZE(alloca_size);" in prepared, \
        "Stack-limit check must account for the padding"
    command = []
    i = 0
    while i < len(original):
        if original[i] == "-o":
            i += 2
            continue
        if original[i] not in ("-c", entry["file"]):
            command.append(original[i])
        i += 1
    with tempfile.TemporaryDirectory(prefix="loka-ppc-stack-") as tmp:
        results = {}
        for name, source in (("fixed", PROBE),
                             ("control", PROBE.replace("SMIRKYCARD_JS_ALLOCA(size)",
                                                       "__builtin_alloca(size)"))):
            path = Path(tmp) / (name + ".c")
            asm = path.with_suffix(".s")
            path.write_text(source)
            subprocess.run(command + ["-S", "-Os", str(path), "-o", str(asm)],
                           cwd=entry["directory"], check=True)
            results[name] = offsets(asm.read_text())
        dynamic, guard = results["fixed"]
        # For a multiple-of-16 allocation, its end is old-SP + dynamic.
        # Fixed locals begin at old-SP + guard; equality is disjoint.
        assert dynamic <= guard, ("alloca overlaps fixed locals", results)
        control_dynamic, control_guard = results["control"]
        if control_dynamic > control_guard:
            print("Control reproduces %d-byte overlap" % (control_dynamic - control_guard))
        else:
            print("Control no longer overlaps; review whether the workaround is still needed")
        print("PASS: payload ends at +%d, fixed locals start at +%d" %
              (dynamic, guard))


if __name__ == "__main__":
    main()
