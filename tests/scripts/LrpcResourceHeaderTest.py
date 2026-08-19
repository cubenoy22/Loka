#!/usr/bin/env python3

import os
import pathlib
import subprocess
import sys
import tempfile


def fail(message):
    raise SystemExit("LrpcResourceHeaderTest: " + message)


def run(command, expected=0, env=None):
    completed = subprocess.run(command, text=True, capture_output=True, env=env)
    if completed.returncode != expected:
        fail(
            "expected exit {}, got {}\nstdout:\n{}\nstderr:\n{}".format(
                expected, completed.returncode, completed.stdout, completed.stderr
            )
        )
    return completed


def main():
    if len(sys.argv) != 2:
        fail("expected <lrpc>")
    lrpc = pathlib.Path(sys.argv[1])

    with tempfile.TemporaryDirectory(prefix="loka-lrpc-header-") as root_text:
        root = pathlib.Path(root_text)
        payload = root / "icon.bin"
        manifest = root / "manifest.txt"
        package = root / "ASSETS.LRP"
        header = root / "R.hpp"
        stamp = root / "ASSETS.stamp"
        payload.write_bytes(b"resource bytes")
        manifest.write_text(
            "bag Main\nasset 42 image UI/Icon icon.bin\n", encoding="ascii"
        )

        run(
            [
                str(lrpc),
                "pack",
                str(manifest),
                "-o",
                str(package),
                "--header",
                str(header),
                "--stamp",
                str(stamp),
            ]
        )
        generated = header.read_text(encoding="ascii")
        if "const AssetRef Icon = {42UL, 0," not in generated:
            fail("generated header does not contain the typed UI/Icon symbol")
        if not package.is_file():
            fail("pack did not commit ASSETS.LRP")
        if (
            package.with_name(package.name + ".tmp").exists()
            or header.with_name(header.name + ".tmp").exists()
            or stamp.with_name(stamp.name + ".tmp").exists()
        ):
            fail("successful pack stranded a staging file")

        # A generated output cannot alias an input. The check happens before
        # any staging file is created, so the manifest remains byte-identical.
        original_manifest = manifest.read_bytes()
        collision_package = root / "collision.LRP"
        refused = run(
            [
                str(lrpc),
                "pack",
                str(manifest),
                "-o",
                str(collision_package),
                "--header",
                str(manifest),
            ],
            expected=1,
        )
        if "would overwrite the input" not in refused.stderr:
            fail("input/header collision did not report the protected input")
        if manifest.read_bytes() != original_manifest or collision_package.exists():
            fail("refused input/header collision changed an input or output")

        # Invalid C++ names fail before replacing either previously good
        # artifact. This is the no-sanitize wall: ambiguity is a build error.
        manifest.write_text(
            "bag Main\nasset 42 image UI/bad-name icon.bin\n", encoding="ascii"
        )
        old_package = package.read_bytes()
        old_header = header.read_bytes()
        refused = run(
            [
                str(lrpc),
                "pack",
                str(manifest),
                "-o",
                str(package),
                "--header",
                str(header),
            ],
            expected=1,
        )
        if "portable C++ symbol path" not in refused.stderr:
            fail("bad resource symbol did not report the header contract")
        if package.read_bytes() != old_package or header.read_bytes() != old_header:
            fail("bad resource symbol replaced a previously good artifact")

        # Root values would collide with R's package-wide AssetCount and
        # Assets vocabulary. Refuse the ambiguous surface before committing.
        manifest.write_text(
            "bag Main\nasset 42 image Flat icon.bin\n", encoding="ascii"
        )
        refused = run(
            [
                str(lrpc),
                "pack",
                str(manifest),
                "-o",
                str(package),
                "--header",
                str(header),
            ],
            expected=1,
        )
        if "must include a namespace" not in refused.stderr:
            fail("flat resource symbol did not report the namespace contract")
        if package.read_bytes() != old_package or header.read_bytes() != old_header:
            fail("flat resource symbol replaced a previously good artifact")

        # Pin the unavoidable multi-file commit failure: once the package has
        # landed, neither an old header nor an old derived stamp may survive.
        manifest.write_text(
            "bag Main\nasset 43 image UI/NewIcon icon.bin\n", encoding="ascii"
        )
        environment = os.environ.copy()
        environment["LOKA_LRPC_TEST_FAIL_COMMIT_PATH"] = str(header)
        failed = run(
            [
                str(lrpc),
                "pack",
                str(manifest),
                "-o",
                str(package),
                "--header",
                str(header),
                "--stamp",
                str(stamp),
            ],
            expected=1,
            env=environment,
        )
        if "cannot commit resource header" not in failed.stderr:
            fail("injected header commit failure did not reach the intended door")
        if package.read_bytes() == old_package:
            fail("injected header commit failure did not follow package commit")
        if header.exists() or header.is_symlink():
            fail("header commit failure left a stale header")
        if stamp.exists() or stamp.is_symlink():
            fail("header commit failure left a stale companion stamp")
        if (
            package.with_name(package.name + ".tmp").exists()
            or header.with_name(header.name + ".tmp").exists()
            or stamp.with_name(stamp.name + ".tmp").exists()
        ):
            fail("header commit failure stranded a staging file")


if __name__ == "__main__":
    main()
