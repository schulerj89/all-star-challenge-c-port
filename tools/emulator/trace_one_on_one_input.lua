-- Headless Mesen trace for the bank-1 $702D player input/shot-phase path.
-- The pre-game pulse sequence only advances menus. At the first execution of
-- $702D the script captures a savestate, traces the two-press A path, restores
-- that state, and traces A-gather/B-release through the one-frame latch.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local mem = emu.memType.gameboyDebug
local reached = {}
local rosterChooseCount = 0
local opponentMoved = false
local initialGameplayState = nil
local reloadRequested = false
local scenario = 1
local failures = 0
local stopping = false

local function read(address)
  return emu.read(address, mem, false)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function onPlayerInputUpdate()
  if reloadRequested then
    reloadRequested = false
    scenario = 2
    gameplayFrames = 0
    emu.loadSavestate(initialGameplayState)
    return
  end

  if initialGameplayState == nil then
    initialGameplayState = emu.createSavestate()
  end
  gameplayReached = true
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
  print(string.format("ROUTE frame=%d roster_choose=%d", totalFrames,
    rosterChooseCount))
end

local function mark(name)
  return function()
    if not reached[name] then
      reached[name] = true
      print(string.format(
        "ROUTE frame=%d pc=%s mode=%02X action=%02X input=%02X",
        totalFrames, name, read(0xFF8F), read(0xFF9D), read(0xFF8B)))
    end
  end
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }

  if not gameplayReached then
    -- The ROM's menu/settings confirmation path checks input bit 3 (Start).
    -- Do not combine it with A: the title treats A as a player-count toggle
    -- before it evaluates Start.
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
    if scenario == 1 then
      input.a = gameplayFrames == 10 or gameplayFrames == 20
    else
      input.a = gameplayFrames == 10
      input.b = gameplayFrames == 20
    end
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if gameplayReached then
    print(string.format(
      "TRACE scenario=%s frame=%d A=%d B=%d owner=%02X p1_action=%02X " ..
      "p1_new=%02X p1_held=%02X p1_phase=%02X latch=%02X ball_vz=%02X%02X",
      scenario == 1 and "A-A" or "A-B", gameplayFrames,
      ((scenario == 1 and (gameplayFrames == 10 or gameplayFrames == 20)) or
       (scenario == 2 and gameplayFrames == 10)) and 1 or 0,
      (scenario == 2 and gameplayFrames == 20) and 1 or 0,
      read(0xFFCF), read(0xFF9D), read(0xFFAE), read(0xFFAF),
      read(0xFFB0), read(0xC16A), read(0xC0A9), read(0xC0A8)))
    if scenario == 1 and gameplayFrames == 10 then
      expect(read(0xFF9D) == 0x0A and read(0xFFCF) == 1 and
             read(0xFFAE) == 1 and read(0xFFAF) == 1 and
             read(0xFFB0) == 0 and read(0xC16A) == 0,
             "first A did not enter the phase-zero $0A gather")
    elseif scenario == 1 and gameplayFrames == 20 then
      expect(read(0xFFCF) == 0 and read(0xFFAE) == 1 and
             read(0xFFB0) == 0 and read(0xC16A) == 0 and
             (read(0xC0A9) ~= 0 or read(0xC0A8) ~= 0),
             "second new-A did not launch directly from phase zero")
    elseif scenario == 2 and gameplayFrames == 20 then
      expect(read(0xFFCF) == 1 and read(0xFFAF) == 2 and
             read(0xFFB0) == 1 and read(0xC16A) == 1,
             "held-B did not arm phase one and the $C16A latch")
    elseif scenario == 2 and gameplayFrames == 21 then
      expect(read(0xFFCF) == 0 and read(0xFFB0) == 2 and
             read(0xC16A) == 0 and
             (read(0xC0A9) ~= 0 or read(0xC0A8) ~= 0),
             "$C16A did not expire into the phase-two launch")
    end
    if scenario == 1 and gameplayFrames >= 25 then
      reloadRequested = true
    elseif scenario == 2 and gameplayFrames >= 26 and not stopping then
      stopping = true
      print(failures == 0 and "TRACE PASSED: $702D A-A and A-B release paths" or
            string.format("TRACE FAILED: %d mismatch(es)", failures))
      emu.stop(failures == 0 and 0 or 3)
    end
  elseif totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(
  onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(mark("$03A1 menu"), emu.callbackType.exec,
  0x03A1, 0x03A1, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(mark("$22EF settings"), emu.callbackType.exec,
  0x22EF, 0x22EF, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(mark("$0B80 match"), emu.callbackType.exec,
  0x0B80, 0x0B80, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(mark("bank2 $4000 roster"), emu.callbackType.exec,
  0x4000, 0x4000, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(mark("bank2 $4113 accept"), emu.callbackType.exec,
  0x4113, 0x4113, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
