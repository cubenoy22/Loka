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
local oKey = field(":macadb:KEY1", "o  O")
local commandKey = field(":macadb:KEY3", "Command / Open Apple")

-- Keys live spread across the :macadb:KEY* ports; find one by field name.
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

say("booting (%s emulated seconds)", BOOT_WAIT)
emu.wait(BOOT_WAIT)
tap(lKey)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
emu.wait(5)
-- Select only the application. A select-all would also open the plain data
-- files beside it, and their "application not found" alerts land on top of
-- the scene the snapshot is supposed to capture. Letter type-selection is
-- unusable here: on a KanjiTalk system the input method swallows typed
-- romaji into its kana window and the Finder never sees it. Tab is immune;
-- it cycles the Finder selection. Empirically (per-tab snapshot diagnostics,
-- 2026-07-31) the first Tab in the freshly opened LokaDev window lands on
-- LokaTestsToolbox68K, then cycles ASSETS.LRP -> LokaTest.cfg -> app again,
-- so the default is one press with run-scenario.sh's three-item staging.
local tabKey = keyByName("Tab")
local tabCount = tonumber(os.getenv("LOKA_TAB_COUNT") or "1")
for _ = 1, tabCount do
    tap(tabKey)
end
emu.wait(1)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()
say("application opened; settling %s emulated seconds", SETTLE_WAIT)
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
