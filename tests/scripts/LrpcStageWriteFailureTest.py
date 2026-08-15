#!/usr/bin/env python3

import pathlib
import resource
import signal
import subprocess
import sys
import tempfile


def fail(message):
    raise SystemExit("LrpcStageWriteFailureTest: " + message)


def limit_output_to_one_byte():
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
    resource.setrlimit(resource.RLIMIT_FSIZE, (1, 1))


def main():
    if len(sys.argv) != 3:
        fail("expected <lrpc> <valid-package>")

    lrpc = pathlib.Path(sys.argv[1])
    source = pathlib.Path(sys.argv[2])
    with tempfile.TemporaryDirectory(prefix="loka-lrpc-stage-write-") as root:
        output = pathlib.Path(root) / "staged.LRP"
        temporary = pathlib.Path(str(output) + ".tmp")
        failed = subprocess.run(
            [str(lrpc), "stage", str(source), "-o", str(output)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            preexec_fn=limit_output_to_one_byte,
        )
        if failed.returncode == 0:
            fail("the one-byte file limit did not force the staged write to fail")
        if temporary.exists() or temporary.is_symlink():
            fail("the failed invocation left its partially written temporary")

        retried = subprocess.run(
            [str(lrpc), "stage", str(source), "-o", str(output)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if retried.returncode != 0:
            fail("the second stage attempt failed: " + retried.stderr.strip())
        if output.read_bytes() != source.read_bytes():
            fail("the successful retry did not produce a byte-identical package")


if __name__ == "__main__":
    main()
