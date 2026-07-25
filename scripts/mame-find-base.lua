-- Report the runtime load base of a Retro68 68K application running in MAME.
--
-- gdb needs one number to make a Retro68 image debuggable: the address the
-- CODE resources were loaded at. The image is linked contiguously from vaddr
-- 0, so `add-symbol-file <elf> -o <base>` relocates every section at once.
--
-- Method: search the application heap for the opening bytes of a function
-- that carries no relocations in the linked ELF. Those bytes are identical in
-- the file and in memory, so a match gives base = found - link_address.
-- Picking a relocation-free function is what makes this independent of
-- Retro68's startup fixups; see docs/MAME_DEVELOPMENT.md for how to choose
-- one with readelf.
--
-- Environment:
--   LOKA_PATTERN_WORD0  first 4 pattern bytes as hex   (required)
--   LOKA_PATTERN_WORD1  next 4 pattern bytes as hex    (required)
--   LOKA_PATTERN_LINK   the function's link address    (required)
--   LOKA_LAUNCH_WAIT    emulated seconds to wait for boot (default 90)
--   LOKA_GDB_LOG        log file path (default mame-find-base.log)
--   LOKA_STAY_ALIVE     set to keep the machine running after reporting,
--                       so a gdb session can attach in the same run
--
-- MAME's Lua is 5.4: string.format("%d", x) raises on a non-integral float,
-- which kills this coroutine while MAME keeps running at full speed. Every
-- division that reaches %d below uses // for that reason.

local function required(name)
    return assert(os.getenv(name), name .. " is required")
end

local WORD0 = tonumber(required("LOKA_PATTERN_WORD0"), 16)
local WORD1 = tonumber(required("LOKA_PATTERN_WORD1"), 16)
local LINK_ADDRESS = tonumber(required("LOKA_PATTERN_LINK"), 16)
local BOOT_WAIT = tonumber(os.getenv("LOKA_LAUNCH_WAIT") or "90")

local log = assert(io.open(os.getenv("LOKA_GDB_LOG") or "mame-find-base.log", "w"))
local function say(fmt, ...)
    local line = "LOKA-BASE: " .. string.format(fmt, ...)
    print(line)
    log:write(line .. "\n")
    log:flush()
end

local function field(portTag, name)
    return assert(manager.machine.ioport.ports[portTag].fields[name])
end

local lKey = field(":macadb:KEY2", "l  L")
local aKey = field(":macadb:KEY0", "a  A")
local oKey = field(":macadb:KEY1", "o  O")
local escapeKey = field(":macadb:KEY3", "Esc")
local commandKey = field(":macadb:KEY3", "Command / Open Apple")

local function tap(key)
    key:set_value(1)
    emu.wait(0.10)
    key:clear_value()
    emu.wait(0.20)
end

local space = manager.machine.devices[":maincpu"].spaces["program"]

-- Boot, open the LokaDev volume, select all, open the application.
say("booting (%d emulated seconds)", BOOT_WAIT)
emu.wait(BOOT_WAIT)
tap(lKey)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
emu.wait(5)
tap(escapeKey)
commandKey:set_value(1); tap(aKey); commandKey:clear_value()
emu.wait(1)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
emu.wait(20)
say("application launched")

-- The CODE resources live in the application heap, which the app's SIZE
-- partition keeps far smaller than RAM. Bound the search with it.
local applZone = space:read_u32(0x02AA)
local applLimit = space:read_u32(0x0130)
say("ApplZone 0x%08x  ApplLimit 0x%08x", applZone, applLimit)

local function scan(from, to)
    local address = from - (from % 2)
    while address < to - 8 do
        if space:read_u32(address) == WORD0 and space:read_u32(address + 4) == WORD1 then
            return address
        end
        address = address + 2
    end
    return nil
end

local found
if applZone > 0x1000 and applLimit > applZone and applLimit < 0x00800000 then
    say("searching the application heap (%d KB)", (applLimit - applZone) // 1024)
    found = scan(applZone, applLimit)
end
if not found then
    say("falling back to a full RAM sweep")
    found = scan(0x00001000, 0x007ffff0)
end

if not found then
    say("pattern NOT found; check that the pattern matches this build")
    log:close()
    manager.machine:exit()
    return
end

local base = found - LINK_ADDRESS
say("found at 0x%08x", found)
say("BASE 0x%08x", base)
say("gdb: add-symbol-file <elf> -o 0x%08x", base)

if os.getenv("LOKA_STAY_ALIVE") then
    say("staying alive for a gdb session")
    while true do
        emu.wait(5)
    end
end

log:close()
manager.machine:exit()
