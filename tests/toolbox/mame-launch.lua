-- Boot, launch the app on the LokaDev disk with the keyboard, then take a
-- screen snapshot as verification evidence and exit. Keyboard sequence and
-- Lua 5.4 cautions follow scripts/mame-find-base.lua.

local BOOT_WAIT = tonumber(os.getenv("LOKA_LAUNCH_WAIT") or "90")
local SETTLE_TIMEOUT = tonumber(os.getenv("LOKA_SETTLE_TIMEOUT") or "30")
local SETTLE_SAMPLE_WAIT = 0.5
local SETTLE_STABLE_SAMPLES = 3

assert(SETTLE_TIMEOUT > 0, "LOKA_SETTLE_TIMEOUT must be positive")

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

local function captureSnapshotPixels(video)
    local width, height = video:snapshot_size()
    return video:snapshot_pixels(), width, height
end

local function snapshotPixel(pixels, width, x, y)
    local offset = (y * width + x) * 4 + 1
    return string.unpack("=I4", pixels, offset)
end

local function completionSignalPixels(pixels, width, height)
    return {
        snapshotPixel(pixels, width, width - 11, height - 11),
        snapshotPixel(pixels, width, width - 8, height - 11),
        snapshotPixel(pixels, width, width - 11, height - 8),
        snapshotPixel(pixels, width, width - 8, height - 8)
    }
end

local function completionSignalVisible(frame, width, height, baseline)
    local signal = completionSignalPixels(frame, width, height)
    local solid = signal[1] == signal[2] and signal[1] == signal[3] and signal[1] == signal[4]
    local changed = signal[1] ~= baseline[1] or signal[2] ~= baseline[2]
        or signal[3] ~= baseline[3] or signal[4] ~= baseline[4]
    return solid and changed, signal
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
local screen = findScreen()
local video = manager.machine.video
local baselineFrame, baselineWidth, baselineHeight = captureSnapshotPixels(video)
local baselineSignal = completionSignalPixels(baselineFrame, baselineWidth, baselineHeight)
commandKey:set_value(1); tap(oKey); commandKey:clear_value()

-- Completion transport: reading a marker from the development HFS disk while
-- the guest owns it would add unsafe cross-mount coordination. After both
-- synchronous record writes return, the test app opens an owned black marker
-- outside the capture crop.
-- MAME observes that signal through its existing live-screen seam, then waits
-- for consecutive identical full-screen samples. This is bounded and fails
-- closed; it never falls back to a fixed capture delay.
say("application opened; waiting up to %.2f emulated seconds for stable pixels", SETTLE_TIMEOUT)
local elapsed = 0.0
local completionSignaled = false
local completionPixels = baselineSignal
local stableSamples = 0
local previousPixels = nil
local previousWidth = 0
local previousHeight = 0
while elapsed < SETTLE_TIMEOUT and stableSamples < SETTLE_STABLE_SAMPLES do
    emu.wait(SETTLE_SAMPLE_WAIT)
    elapsed = elapsed + SETTLE_SAMPLE_WAIT
    local pixels, width, height = captureSnapshotPixels(video)
    if width == baselineWidth and height == baselineHeight then
        local visible = false
        visible, completionPixels = completionSignalVisible(pixels, width, height, baselineSignal)
        completionSignaled = completionSignaled or visible
    end
    if completionSignaled then
        if previousPixels == pixels and previousWidth == width and previousHeight == height then
            stableSamples = stableSamples + 1
        else
            stableSamples = 1
        end
    end
    previousPixels = pixels
    previousWidth = width
    previousHeight = height
end
if stableSamples < SETTLE_STABLE_SAMPLES then
    say("settle failed after %.2f emulated seconds (completion=%s stable=%d/%d)",
        elapsed, tostring(completionSignaled), stableSamples, SETTLE_STABLE_SAMPLES)
    say("completion pixels current=%s,%s,%s,%s baseline=%s,%s,%s,%s",
        tostring(completionPixels[1]), tostring(completionPixels[2]),
        tostring(completionPixels[3]), tostring(completionPixels[4]),
        tostring(baselineSignal[1]), tostring(baselineSignal[2]),
        tostring(baselineSignal[3]), tostring(baselineSignal[4]))
    local snapshotOk, snapshotError = pcall(function() screen:snapshot() end)
    say("failure snapshot %s%s", snapshotOk and "written" or "failed",
        snapshotOk and "" or ": " .. tostring(snapshotError))
    manager.machine:exit()
    return
end
say("settled after %.2f emulated seconds (%d stable samples)", elapsed, stableSamples)

-- manager.machine.screens is a device enumerator, not a Lua table: index it
-- by tag or iterate with pairs(); next() raises a type error.
local ok, err = pcall(function()
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
