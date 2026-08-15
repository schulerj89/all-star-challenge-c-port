-- Headless Mesen proof for the selected-roster palette path and $7138's
-- forced hoop-facing shot side. The menu route selects roster entries 0/1,
-- which intentionally carry different $2DD2 record skin bytes ($91/$90).

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local initialState = nil
local reloadRequested = false
local scenario = 1
local stopping = false
local failures = 0
local paletteP1 = false
local paletteP2 = false
local facing = {false, false}
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function onP1PaletteComplete()
  if paletteP1 then return end
  paletteP1 = true
  print(string.format("ROSTER_PALETTE p1 record_skin=%02X OBP0=%02X",
    read(0xC23B), read(0xFF48)))
  expect(read(0xC23B) == 0x91 and read(0xFF48) == 0xD9,
    "$21FA P1 light roster record did not select OBP0=$D9")
end

local function onP2PaletteComplete()
  if paletteP2 then return end
  paletteP2 = true
  print(string.format("ROSTER_PALETTE p2 record_skin=%02X OBP1=%02X",
    read(0xC254), read(0xFF49)))
  expect(read(0xC254) == 0x90 and read(0xFF49) == 0xE0,
    "$21FA P2 dark roster record did not select OBP1=$E0")
end

local function onPlayerInputUpdate()
  if reloadRequested then
    reloadRequested = false
    scenario = 2
    gameplayFrames = 0
    emu.loadSavestate(initialState)
    return
  end
  if initialState == nil then initialState = emu.createSavestate() end
  gameplayReached = true
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onShotFacingComplete()
  if not gameplayReached or facing[scenario] then return end
  local rawX = read(P1 + 0x06)
  local flip = (read(P1 + 0x02) & 0x10) ~= 0
  facing[scenario] = true
  print(string.format("SHOT_FACING scenario=%s raw_x=%02X center=%02X flip=%d",
    scenario == 1 and "left" or "right", rawX, (rawX + 8) & 0xFF,
    flip and 1 or 0))
  if scenario == 1 then
    expect(rawX + 8 < 0x54 and flip,
      "$7138 left-side gather did not set player +$02 bit 4")
  else
    expect(rawX + 8 >= 0x54 and not flip,
      "$7138 right-side gather did not clear player +$02 bit 4")
  end
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not gameplayReached then
    if totalFrames % 30 == 1 then
      if rosterChooseCount >= 2 and not opponentMoved then
        input.right = true
        opponentMoved = true
      else
        input.start = true
      end
    end
  else
    gameplayFrames = gameplayFrames + 1
    -- Keep raw player X pinned across the $702D frame. The ROM stores raw X
    -- at +$06 and $7138 adds eight before comparing against $54.
    write(P1 + 0x06, scenario == 1 and 0x20 or 0x80)
    input.a = gameplayFrames == 10
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if gameplayReached and scenario == 1 and gameplayFrames >= 15 and
     facing[1] then
    reloadRequested = true
  elseif gameplayReached and scenario == 2 and gameplayFrames >= 20 and
     not stopping then
    stopping = true
    expect(paletteP1 and paletteP2,
      "$21FA roster palette callbacks were not observed")
    expect(facing[1] and facing[2],
      "$7138 was not observed for both court sides")
    print(failures == 0 and
      "TRACE PASSED: $2DD2->$21FA roster palettes and $7138 shot facing" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onP1PaletteComplete, emu.callbackType.exec,
  0x2207, 0x2207, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onP2PaletteComplete, emu.callbackType.exec,
  0x2214, 0x2214, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onShotFacingComplete, emu.callbackType.exec,
  0x7149, 0x7149, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onShotFacingComplete, emu.callbackType.exec,
  0x714C, 0x714C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
