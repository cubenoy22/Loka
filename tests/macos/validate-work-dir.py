#!/usr/bin/env python3

import os
import sys


def main(arguments):
    if len(arguments) != 2:
        return 2
    build_root = os.path.realpath(arguments[0])
    work = os.path.realpath(arguments[1])
    if work == build_root or not work.startswith(build_root + os.sep):
        return 1
    print(work)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
