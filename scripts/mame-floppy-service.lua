local requestPath = os.getenv("LOKA_MAME_FLOPPY_REQUEST")
local responsePath = os.getenv("LOKA_MAME_FLOPPY_RESPONSE")

if not requestPath or requestPath == ""
    or not responsePath or responsePath == "" then
    error("MAME floppy control paths are not set")
end

local requestedTag = os.getenv("MAME_FLOPPY_DEVICE")
local floppy = nil

if requestedTag and requestedTag ~= "" then
    floppy = manager.machine.images[requestedTag]
else
    for _, image in pairs(manager.machine.images) do
        if image.instance_name == "floppydisk"
            or image.brief_instance_name == "flop" then
            floppy = image
            break
        end
    end
end

if not floppy then
    error("MAME floppy image device was not found")
end

local function respond(status, message)
    local temporaryPath = responsePath .. ".tmp"
    local response = assert(io.open(temporaryPath, "w"))
    response:write(status, "\n", message or "", "\n")
    response:close()
    os.remove(responsePath)
    assert(os.rename(temporaryPath, responsePath))
end

local function processRequest()
    local request = io.open(requestPath, "r")
    if not request then
        return
    end

    local command = request:read("*l")
    local path = request:read("*l")
    request:close()
    os.remove(requestPath)

    if command == "eject" then
        floppy:unload()
        respond("ok", "floppy ejected")
        print("Loka: ejected floppy image")
        return
    end

    if command ~= "mount" or not path or path == "" then
        respond("error", "invalid floppy request")
        return
    end

    floppy:unload()
    local loadError, message = floppy:load(path)
    if loadError then
        local detail = message or tostring(loadError)
        respond("error", detail)
        return
    end

    respond("ok", "mounted " .. path)
    print("Loka: mounted floppy image " .. path)
end

os.remove(requestPath)
os.remove(responsePath)
emu.register_periodic(processRequest)
print("Loka: floppy mount service ready")
