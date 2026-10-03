-- Partition sizing probe (#1103): boot, launch the application on the LokaDev
-- disk (keyboard sequence as in mame-launch.lua), run a step workload, and
-- track the largest unpurgeable footprint of the application zone by walking
-- its 24-bit block list after every step and every 0.25 emulated seconds of a
-- wait. measure-example-heaps.sh owns the workloads and turns the reported
-- peak into a partition need.
--
-- Block accounting (Inside Macintosh: Memory, 24-bit zones): each block
-- starts with a tag byte (bits 7-6: 00 free, 01 nonrelocatable,
-- 10 relocatable) and a 3-byte physical size; a relocatable block's second
-- long is its master pointer's offset from the zone, and the master pointer's
-- high byte carries the locked (0x80) and purgeable (0x40) flags. Live bytes
-- are everything the Memory Manager cannot reclaim: nonrelocatable blocks,
-- locked relocatable blocks (the loaded CODE), and unlocked relocatable blocks
-- that are not purgeable. Lua 5.4 cautions follow scripts/mame-find-base.lua.

local ZONE_HEADER_BYTES = 52 -- Zone record size; heapData starts here.

local log = assert(io.open(os.getenv("LOKA_SNAP_LOG") or "mame-measure-heap.log", "w"))
local function say(fmt, ...)
    log:write(string.format(fmt, ...) .. "\n")
    log:flush()
end

local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]

-- KanjiTalk's input method takes letters and Space; Command-Space switches it
-- to Roman input. Workloads that type start with "K space".
local ALIASES = {
    down = { "Down Arrow", "Down", "Cursor Down" },
    ret = { "Return" },
    space = { "Space" },
    tab = { "Tab" },
    l = { "l  L" },
    o = { "o  O" },
    cmd = { "Command / Open Apple" },
    zero = { "0  )" },
    five = { "5  %" },
}

local function key(name)
    for _, fieldName in ipairs(ALIASES[name] or { name }) do
        for tag, port in pairs(manager.machine.ioport.ports) do
            if tag:find("^:macadb:KEY") and port.fields[fieldName] then
                return port.fields[fieldName]
            end
        end
    end
    error("no key field for " .. name)
end

local function tap(field)
    field:set_value(1)
    emu.wait(0.10)
    field:clear_value()
    emu.wait(0.20)
end

local function commandTap(name)
    local command = key("cmd")
    command:set_value(1)
    tap(key(name))
    command:clear_value()
end

local appZone = nil
local peak = { live = 0, at = "none" }

-- Returns false once the application zone is gone (the app quit or aborted).
local function walk(label, verbose)
    local zone = mem:read_u32(0x2AA) -- ApplZone
    if appZone and zone ~= appZone then
        say("%s application zone gone (%08x -> %08x)", label, appZone, zone)
        return false
    end
    local bkLim = mem:read_u32(zone)
    local address = zone + ZONE_HEADER_BYTES
    local free, nonrel, locked, unlocked, purgeable, blocks = 0, 0, 0, 0, 0, 0
    while address < bkLim do
        local header = mem:read_u32(address)
        local tag = (header >> 30) & 3
        local size = header & 0xFFFFFF
        if size < 8 or address + size > bkLim + 16 then
            error(string.format("%s: corrupt block header %08x at %08x", label, header, address))
        end
        if tag == 0 then
            free = free + size
        elseif tag == 1 then
            nonrel = nonrel + size
        elseif tag == 2 then
            local flags = mem:read_u8(zone + mem:read_u32(address + 4))
            if flags & 0x80 ~= 0 then
                locked = locked + size
            elseif flags & 0x40 ~= 0 then
                purgeable = purgeable + size
            else
                unlocked = unlocked + size
            end
        end
        blocks = blocks + 1
        address = address + size
    end
    local live = nonrel + locked + unlocked
    if live > peak.live then
        peak.live = live
        peak.at = label
    end
    if verbose then
        say("%s zone=%08x size=%d max=%d blocks=%d free=%d nonrel=%d locked=%d unlocked=%d purgeable=%d live=%d",
            label, zone, bkLim - zone, mem:read_u32(0x130) - zone, blocks, free, nonrel, locked, unlocked,
            purgeable, live)
    end
    return true
end

local couple = 1
local function warp(h, v)
    mem:write_u16(0x828, v) -- MTemp
    mem:write_u16(0x82A, h)
    mem:write_u16(0x82C, v) -- RawMouse
    mem:write_u16(0x82E, h)
    mem:write_u8(0x8CE, couple) -- CrsrNew := CrsrCouple
    emu.wait(0.5)
end

local button = manager.machine.ioport.ports[":macadb:MOUSE0"].fields["Mouse Button 0"]

local function sampledWait(seconds, label)
    local elapsed = 0
    while elapsed < seconds do
        emu.wait(0.25)
        elapsed = elapsed + 0.25
        walk(label, false)
    end
end

local ok, err = pcall(function()
    emu.wait(tonumber(os.getenv("LOKA_LAUNCH_WAIT") or "90"))
    local storedCouple = mem:read_u8(0x8CF)
    if storedCouple ~= 0 then
        couple = storedCouple
    end
    tap(key("l"))
    commandTap("o")
    emu.wait(5)
    local tabs = assert(tonumber(os.getenv("LOKA_TAB_COUNT")), "LOKA_TAB_COUNT must be set by the runner")
    for _ = 1, tabs do
        tap(key("tab"))
    end
    emu.wait(1)
    commandTap("o")
    emu.wait(15)
    appZone = mem:read_u32(0x2AA)
    walk("launched", true)
    manager.machine.video:snapshot()

    -- Steps, each ending in ';': "c H V" click, "k NAME" key, "K NAME"
    -- Command-key, "w SECONDS" sampled wait, "s" logged walk plus snapshot.
    local index = 0
    for op, first, second in (os.getenv("LOKA_STEPS") or ""):gmatch("(%a)%s*([^%s;]*)%s*([^;]*);") do
        index = index + 1
        local label = string.format("step%03d-%s", index, op)
        if op == "c" then
            warp(tonumber(first), tonumber(second))
            button:set_value(1)
            emu.wait(0.2)
            button:clear_value()
            sampledWait(0.5, label)
        elseif op == "k" then
            tap(key(first))
            walk(label, false)
        elseif op == "K" then
            commandTap(first)
            sampledWait(1, label)
        elseif op == "w" then
            sampledWait(tonumber(first), label)
        elseif op == "s" then
            walk(label, true)
            manager.machine.video:snapshot()
        else
            error("unknown step " .. op)
        end
    end
    if walk("end", true) then
        manager.machine.video:snapshot()
        say("PEAK live=%d at=%s", peak.live, peak.at)
    end
    say("LOKA-MEASURE: complete")
end)
if not ok then
    say("LOKA-MEASURE: lua error: %s", tostring(err))
end
manager.machine:exit()
