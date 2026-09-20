local root = "C:/Users/cuben/Documents/GitHub/Loka/build/smirky-plus-verify-837/"
local log = assert(io.open(root .. "probe.log", "w"))
local function say(s) log:write(s .. "\n"); log:flush(); print(s) end
local cpu = manager.machine.devices[":maincpu"]
local mem = cpu.spaces["program"]
local screen
for _,s in pairs(manager.machine.screens) do screen=s; break end
local function snap(name) screen:snapshot(root .. name .. ".png") end
local function key(name)
  for tag,p in pairs(manager.machine.ioport.ports) do
    for n,f in pairs(p.fields) do
      if n == name then return f end
    end
  end
  error("key missing: " .. name)
end
local function tap(k)
  k:set_value(1); emu.wait(0.5); k:clear_value(); emu.wait(0.5)
end
local function open()
  local cmd=key("Command")
  cmd:set_value(1); emu.wait(0.5); tap(key("o  O")); cmd:clear_value(); emu.wait(4)
end
local tripped=false
local tapper
local function capture()
  say("ADDRESS ERROR")
  for n,s in pairs(cpu.state) do say(n .. "=" .. string.format("%x",s.value)) end
  local sp=cpu.state["SP"].value
  for i=0,64,2 do say(string.format("SP+%02x %04x",i,mem:read_u16(sp+i))) end
  local f=assert(io.open(root.."ram.bin","wb"))
  f:write(mem:read_range(0,0x3fffff,8)); f:close()
end
local ok,err=pcall(function()
  emu.wait(90); snap("boot")
  tapper=mem:install_read_tap(0x0c,0x0f,"address-error",function()
    if not tripped then tripped=true; capture() end
  end)
  snap("disk")
  manager.machine.natkeyboard:post("Loka"); emu.wait(2); open(); snap("folder")
  mem:write_u16(0x828,195); mem:write_u16(0x82a,168)
  mem:write_u16(0x82c,195); mem:write_u16(0x82e,168)
  mem:write_u16(0x830,195); mem:write_u16(0x832,168)
  mem:write_u8(0x8ce,1); emu.wait(1)
  say("mouse mask="..key("Mouse Button").mask)
  tap(key("Mouse Button")); snap("selected")
  local mouse=key("Mouse Button")
  mouse:set_value(1); emu.wait(0.08); mouse:clear_value(); emu.wait(0.08)
  mouse:set_value(1); emu.wait(0.08); mouse:clear_value(); emu.wait(6)
  snap("launch")
  emu.wait(45); snap("result"); say(tripped and "VERDICT: ADDRESS ERROR" or "VERDICT: NO ADDRESS ERROR")
end)
if not ok then say(tostring(err)) end
if tapper then tapper:remove() end
log:close()
manager.machine:exit()
