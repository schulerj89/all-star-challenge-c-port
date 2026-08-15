-- Headless Mesen assertions for fixed-bank $2C50/$2CCA/$0AC5 contact rules.
-- Each scenario restores one gameplay savestate, injects the exact prior-
-- update +$0C latch at the dispatcher boundary, and observes the cartridge's
-- charging, blocking, and protected-action branches.

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
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local P2 = 0xFFB6
local observed = {charging = false, blocking = false, protected = false}
local contactEntries = 0
local alignmentEntries = 0

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

local function place(base, action, x, y, variant, latch, counter)
  write(base + 0x00, action)
  write(base + 0x06, x)
  write(base + 0x15, y)
  write(base + 0x16, variant)
  write(base + 0x0C, latch)
  write(base + 0x0D, counter)
end

local function onRuleDispatcher()
  gameplayReached = true
  if reloadRequested then
    reloadRequested = false
    emu.loadSavestate(initialState)
    return
  end
  if initialState == nil then initialState = emu.createSavestate() end

  write(0xFFEB, 0)
  write(0xC178, 0)
  write(0xFFCF, 1)
  if scenario == 1 then
    -- Owner P1 at variant zero; P2 is exactly owner.x+$0C.  Expiring
    -- owner contact is charging.
    place(P1, 0x01, 0x40, 0x60, 0, 1, 1)
    place(P2, 0x09, 0x4C, 0x60, 0, 0, 0x19)
  elseif scenario == 2 then
    -- Identical geometry, but the defender's timer expires: blocking.
    place(P1, 0x01, 0x40, 0x60, 0, 0, 0x19)
    place(P2, 0x09, 0x4C, 0x60, 0, 1, 1)
  else
    -- Shot action $0A is protected by $0A78.  $2CCA must clear the old
    -- contact and reload the counter instead of calling $0AC5.
    place(P1, 0x0A, 0x40, 0x60, 0, 1, 1)
    place(P2, 0x09, 0x4C, 0x60, 0, 0, 0x19)
  end
end

local function onCharging()
  if scenario ~= 1 then return end
  observed.charging = true
  expect(read(0xFFD0) == 1, "$2C50 did not retain P1 as charging offender")
  print(string.format(
    "CONTACT charging owner=%02X offender=%02X counter=%02X",
    read(0xFFCF), read(0xFFD0), read(P1 + 0x0D)))
  scenario = 2
  reloadRequested = true
end

local function onBlocking()
  if scenario ~= 2 then return end
  observed.blocking = true
  expect(read(0xFFD0) == 2, "$2C50 did not retain P2 as blocking offender")
  print(string.format(
    "CONTACT blocking owner=%02X offender=%02X counter=%02X",
    read(0xFFCF), read(0xFFD0), read(P2 + 0x0D)))
  scenario = 3
  reloadRequested = true
end

local function onP1ContactReturn()
  if scenario ~= 3 then return end
  if read(P1 + 0x0C) == 0 and read(P1 + 0x0D) == 0x19 then
    observed.protected = true
    print(string.format(
      "CONTACT protected action=%02X latch=%02X counter=%02X",
      read(P1 + 0x00), read(P1 + 0x0C), read(P1 + 0x0D)))
  end
end

local function onContactCounter()
  contactEntries = contactEntries + 1
  if contactEntries <= 6 then
    print(string.format(
      "COUNTER scenario=%d owner=%02X p1=%02X/%02X/%02X p2=%02X/%02X/%02X",
      scenario, read(0xFFCF), read(P1), read(P1 + 0x0C), read(P1 + 0x0D),
      read(P2), read(P2 + 0x0C), read(P2 + 0x0D)))
  end
end

local function onAlignment()
  alignmentEntries = alignmentEntries + 1
  if alignmentEntries <= 6 then
    print(string.format(
      "ALIGN scenario=%d owner=%02X variant=%02X x=%02X/%02X y=%02X/%02X",
      scenario, read(0xFFCF), read(P1 + 0x16), read(P1 + 0x06),
      read(P2 + 0x06), read(P1 + 0x15), read(P2 + 0x15)))
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
  end
  emu.setInput(input, 0)
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onEndFrame()
  if stopping then return end
  if observed.charging and observed.blocking and observed.protected then
    stopping = true
    print(failures == 0 and
      "TRACE PASSED: $2C50/$2CCA/$0AC5 charging and blocking rules" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif gameplayReached and gameplayFrames > 300 then
    stopping = true
    print(string.format("TRACE ERROR: contact scenario %d timed out", scenario))
    emu.stop(2)
  elseif totalFrames >= 3600 then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onRuleDispatcher, emu.callbackType.exec,
  0x2C50, 0x2C50, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCharging, emu.callbackType.exec,
  0x2CF4, 0x2CF4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onBlocking, emu.callbackType.exec,
  0x2D02, 0x2D02, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onP1ContactReturn, emu.callbackType.exec,
  0x2CC5, 0x2CC5, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onContactCounter, emu.callbackType.exec,
  0x2CCA, 0x2CCA, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAlignment, emu.callbackType.exec,
  0x0AC5, 0x0AC5, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
