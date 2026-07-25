# Attach to MAME's 68K gdbstub with Loka symbols relocated to the running
# application. Source it, do not run it standalone:
#
#   LOKA_ELF=<path to *.code.bin.gdb> \
#   LOKA_BASE=0x00XXXXXX \
#   LOKA_BREAK=App::idlePolicy \
#     gdb-multiarch -q -x scripts/mame-attach.gdb
#
# LOKA_BASE comes from scripts/mame-find-base.lua. LOKA_BREAK is optional and
# defaults to a function on the idle path, which is a convenient first stop
# because it runs continuously once the application is up.
#
# Wait for the stub to listen before sourcing this -- attaching too early
# fails with "Connection reset by peer". Use netstat rather than a TCP probe;
# the stub accepts one connection and a probe consumes it.
#
#   until netstat.exe -an | grep -q "23946.*LISTENING"; do sleep 1; done
#   sleep 4

set confirm off
set pagination off
set height 0
set architecture m68k
set endian big
set remotetimeout 120

python
import os
elf  = os.environ.get("LOKA_ELF")
base = os.environ.get("LOKA_BASE")
brk  = os.environ.get("LOKA_BREAK") or "App::idlePolicy"
host = os.environ.get("LOKA_GDB_HOST")
if not elf or not base:
    raise gdb.GdbError("set LOKA_ELF and LOKA_BASE")
if not host:
    # WSL cannot reach the Windows loopback interface; use the default gateway.
    try:
        with open("/proc/net/route") as f:
            for line in f.read().splitlines()[1:]:
                c = line.split()
                if c[1] == "00000000":
                    host = ".".join(str(int(c[2][i:i+2], 16)) for i in (6, 4, 2, 0))
                    break
    except Exception:
        pass
    host = host or "localhost"
port = os.environ.get("LOKA_GDB_PORT") or "23946"

gdb.execute("target remote %s:%s" % (host, port))
# One offset relocates every section: the image is linked contiguously from 0.
gdb.execute("add-symbol-file %s -o %s" % (elf, base))
# An ordinary breakpoint: MAME has no hardware breakpoint support, and does
# not need it. Its software breakpoints are an emulator-side check rather than
# a trap written into memory, so one planted before the application loads
# survives its CODE resources being read in over that address.
gdb.execute("break %s" % brk)
print("LOKA: attached to %s:%s, symbols at %s, waiting for %s" % (host, port, base, brk))
end
