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
    s = { "s  S" },
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

local finderZone = nil
local appZone = nil
local peak = { live = 0, at = "none" }
local tapSamples, tapDiscarded = 0, 0

-- Sum one zone's blocks. Returns nil when the block list does not tile the
-- zone exactly, which is what a walk sees in the middle of a Memory Manager
-- operation.
local function measureZone(zone)
    local bkLim = mem:read_u32(zone)
    local address = zone + ZONE_HEADER_BYTES
    local free, nonrel, locked, unlocked, purgeable, blocks = 0, 0, 0, 0, 0, 0
    while address < bkLim do
        local header = mem:read_u32(address)
        local tag = (header >> 30) & 3
        local size = header & 0xFFFFFF
        if size < 8 or address + size > bkLim then
            return nil
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
        else
            return nil
        end
        blocks = blocks + 1
        address = address + size
    end
    if address ~= bkLim then
        return nil
    end
    return {
        bkLim = bkLim, blocks = blocks, free = free, nonrel = nonrel, locked = locked,
        unlocked = unlocked, purgeable = purgeable, live = nonrel + locked + unlocked,
    }
end

local function notePeak(live, label)
    if live > peak.live then
        peak.live = live
        peak.at = label
    end
end

-- The Process Manager swaps ApplZone on every context switch, so a sample can
-- land while the Finder is current. Walk the application's own zone directly,
-- and call the application gone only when its zone stays away from ApplZone.
local function applicationAlive()
    for _ = 1, 40 do
        if mem:read_u32(0x2AA) == appZone then
            return true
        end
        emu.wait(0.05)
    end
    return false
end

local function walk(label, verbose)
    local m = measureZone(appZone)
    if not m then
        error(string.format("%s: zone %08x does not tile into blocks", label, appZone))
    end
    notePeak(m.live, label)
    if verbose then
        say("%s zone=%08x size=%d blocks=%d free=%d nonrel=%d locked=%d unlocked=%d purgeable=%d live=%d",
            label, appZone, m.bkLim - appZone, m.blocks, m.free, m.nonrel, m.locked, m.unlocked,
            m.purgeable, m.live)
    end
end

-- Transient peaks: an allocation made and released inside one event never
-- shows in a settled walk, so walk the zone on every write to its zcbFree
-- (offset 12), which the Memory Manager updates on each allocation and
-- release. Mid-operation walks that do not tile are discarded.
local zoneTap = nil
local tapLabel = "launch"
local function tapZone(zone)
    zoneTap = mem:install_write_tap(zone + 12, zone + 15, "loka-zcbfree", function(offset, data, mask)
        local m = measureZone(zone)
        if m then
            tapSamples = tapSamples + 1
            notePeak(m.live, tapLabel)
        else
            tapDiscarded = tapDiscarded + 1
        end
        return data
    end)
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
    tapLabel = label
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
    finderZone = mem:read_u32(0x2AA)
    commandTap("o")
    -- Watch the switch into the application's zone closely so the tap sees
    -- its mount, then let the launch settle.
    local waited = 0
    while waited < 15 do
        emu.wait(0.01)
        waited = waited + 0.01
        local zone = mem:read_u32(0x2AA)
        if not zoneTap and zone ~= finderZone then
            appZone = zone
            tapZone(zone)
            say("application zone %08x after %.2f s (Finder zone %08x)", zone, waited, finderZone)
        end
    end
    if not appZone or not applicationAlive() then
        error(string.format("the application is not running after launch (ApplZone %08x, Finder zone %08x)",
            mem:read_u32(0x2AA), finderZone))
    end
    -- ApplLimit belongs to the current process, which applicationAlive just
    -- confirmed is the application.
    say("launched max=%d", mem:read_u32(0x130) - appZone)
    walk("launched", true)
    manager.machine.video:snapshot()

    -- Steps, each ending in ';': "c H V" click, "k NAME" key, "K NAME"
    -- Command-key, "w SECONDS" sampled wait, "s" logged walk plus snapshot.
    local index = 0
    for op, first, second in (os.getenv("LOKA_STEPS") or ""):gmatch("(%a)%s*([^%s;]*)%s*([^;]*);") do
        index = index + 1
        local label = string.format("step%03d-%s", index, op)
        tapLabel = label
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
    if applicationAlive() then
        walk("end", true)
        manager.machine.video:snapshot()
        say("tap samples=%d discarded=%d", tapSamples, tapDiscarded)
        say("PEAK live=%d at=%s", peak.live, peak.at)
    else
        say("end application zone gone (ApplZone %08x)", mem:read_u32(0x2AA))
    end
    say("LOKA-MEASURE: complete")
end)
if not ok then
    say("LOKA-MEASURE: lua error: %s", tostring(err))
end
manager.machine:exit()
