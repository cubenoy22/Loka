-- Boot, launch the loop presentation app on the LokaDev disk, then snapshot
-- the screen on a fixed cadence so a full reel can be assembled into footage
-- (tests/scenarios/pngtool.py gif).
--
-- Deliberately separate from tests/toolbox/mame-launch.lua rather than an
-- option on it. That script is the scenario rail's driver: run-scenario.sh
-- picks the newest PNG in the snapshot directory as the capture to compare
-- against a golden, so a periodic-snapshot branch living there would put a
-- second failure mode inside the golden path (#399) for a presentation-only
-- feature. The shared fact the two files both encode is the Finder
-- navigation prologue -- boot wait, type-select the volume, Cmd-O, Tab to the
-- application, Cmd-O -- and it is duplicated on purpose: if the boot disk's
-- System or the dev disk's file layout changes, BOTH scripts must be updated
-- together. Extracting the prologue into one shared Lua file is the right end
-- state and is recorded as follow-up work on #402.
--
-- Unlike the rail driver there is no completion marker and no verdict: a
-- presentation build never signals settle. Recording stops on a wall of
-- emulated time, and MAME exits cleanly so the snapshot files are flushed.

local BOOT_WAIT = tonumber(os.getenv("LOKA_LAUNCH_WAIT") or "90")
local RECORD_SECONDS = tonumber(os.getenv("LOKA_RECORD_SECONDS") or "60")
local FRAME_INTERVAL = tonumber(os.getenv("LOKA_RECORD_INTERVAL") or "0.5")

assert(RECORD_SECONDS > 0, "LOKA_RECORD_SECONDS must be positive")
assert(FRAME_INTERVAL > 0, "LOKA_RECORD_INTERVAL must be positive")

local log = assert(io.open(os.getenv("LOKA_SNAP_LOG") or "mame-loop-record.log", "w"))
local function say(fmt, ...)
    local line = "LOKA-LOOP: " .. string.format(fmt, ...)
    print(line)
    log:write(line .. "\n")
    log:flush()
end

local function field(portTag, name)
    return assert(manager.machine.ioport.ports[portTag].fields[name])
end

local lKey = field(":macadb:KEY2", "l  L")
local oKey = field(":macadb:KEY1", "o  O")
local commandKey = field(":macadb:KEY3", "Command / Open Apple")

local function keyByName(name)
    for tag, port in pairs(manager.machine.ioport.ports) do
        if tag:find("^:macadb:KEY") then
            local found = port.fields[name]
            if found then
                return found
            end
        end
    end
    error("no key field named " .. name)
end

local function tap(key)
    key:set_value(1)
    emu.wait(0.10)
    key:clear_value()
    emu.wait(0.20)
end

local function findScreen()
    local screen = manager.machine.screens[":screen"]
    if not screen then
        for tag, candidate in pairs(manager.machine.screens) do
            say("found screen %s", tostring(tag))
            screen = candidate
            break
        end
    end
    assert(screen, "no screen device")
    return screen
end

say("booting (%s emulated seconds)", BOOT_WAIT)
emu.wait(BOOT_WAIT)
tap(lKey)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
emu.wait(5)

local tabKey = keyByName("Tab")
local tabCount = assert(tonumber(os.getenv("LOKA_TAB_COUNT")),
    "LOKA_TAB_COUNT must be set by the caller")
assert(tabCount > 0 and tabCount == math.floor(tabCount),
    "LOKA_TAB_COUNT must be a positive integer")
for _ = 1, tabCount do
    tap(tabKey)
end
emu.wait(1)

local screen = findScreen()
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
say("opened application; recording %s emulated seconds every %s",
    RECORD_SECONDS, FRAME_INTERVAL)

local elapsed = 0
local frames = 0
while elapsed < RECORD_SECONDS do
    screen:snapshot()
    frames = frames + 1
    emu.wait(FRAME_INTERVAL)
    elapsed = elapsed + FRAME_INTERVAL
end

say("recorded %d frames over %s emulated seconds", frames, elapsed)
emu.wait(1)
manager.machine:exit()
