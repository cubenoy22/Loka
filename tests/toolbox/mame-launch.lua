-- Boot, launch the app on the LokaDev disk with the keyboard, then take a
-- screen snapshot as verification evidence and exit. Keyboard sequence and
-- Lua 5.4 cautions follow scripts/mame-find-base.lua.

local BOOT_WAIT = tonumber(os.getenv("LOKA_LAUNCH_WAIT") or "90")
local SETTLE_WAIT = tonumber(os.getenv("LOKA_SETTLE_WAIT") or "30")

local log = assert(io.open(os.getenv("LOKA_SNAP_LOG") or "mame-launch.log", "w"))
local function say(fmt, ...)
    local line = "LOKA-SNAP: " .. string.format(fmt, ...)
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

say("booting (%d emulated seconds)", BOOT_WAIT)
emu.wait(BOOT_WAIT)
tap(lKey)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
emu.wait(5)
tap(escapeKey)
commandKey:set_value(1); tap(aKey); commandKey:clear_value()
emu.wait(1)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
say("application opened; settling %d emulated seconds", SETTLE_WAIT)
emu.wait(SETTLE_WAIT)

-- manager.machine.screens is a device enumerator, not a Lua table: index it
-- by tag or iterate with pairs(); next() raises a type error.
local ok, err = pcall(function()
    local screen = manager.machine.screens[":screen"]
    if not screen then
        for tag, s in pairs(manager.machine.screens) do
            say("found screen %s", tostring(tag))
            screen = s
            break
        end
    end
    assert(screen, "no screen device")
    screen:snapshot()
end)
if ok then
    say("snapshot written")
else
    say("snapshot failed: %s", tostring(err))
end
emu.wait(1)
say("done")
manager.machine:exit()
