-- Headless Mesen trace for the fixed-bank $2B14 steal path and the
-- $70BF->$2B6C->$2B88 defensive-jump recovery path.  The script drives the
-- original cartridge into One-on-One, then injects deterministic contact
-- states at the native routine boundaries so every branch is reproducible.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local mem = emu.memType.gameboyDebug
local rosterChooseCount = 0
local opponentMoved = false
local failures = 0
local stopping = false
local stealArmed = false
local stealReached = false
local liveJumpReached = false
local reboundJumpReached = false
local scenario = 1
local reboundPlayer = 0

local P1 = 0xFF9D
local P2 = 0xFFB6

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

local function placePlayer(base, action, x, visualY, groundY, facing, noBall)
  write(base + 0x00, action)
  write(base + 0x05, visualY)
  write(base + 0x06, x)
  write(base + 0x09, noBall and 1 or 0)
  write(base + 0x15, groundY)
  write(base + 0x10, facing)
end

local function placeBall(x, y, z)
  write(0xC0A3, x)
  write(0xC0A7, y)
  write(0xC0AB, z)
end

local function clearTransferLocks(firstContact)
  write(0xFFE2, 0)
  write(0xFFE7, 0)
  write(0xFFF8, firstContact and 1 or 0)
  write(0xC12D, 0)
end

local function onPlayerInputUpdate()
  gameplayReached = true
  if scenario == 1 and not stealArmed and read(0xC187) == 1 then
    -- P1 defends P2.  $2B14 accepts a vulnerable action, strict $077D
    -- contact, and opposing horizontal facing masks (1|2 == 3).
    placePlayer(P1, 0x00, 0x40, 0x60, 0x70, 0x01, true)
    placePlayer(P2, 0x00, 0x40, 0x60, 0x70, 0x02, false)
    placeBall(0x48, 0x6E, 0)
    write(0xFFCF, 2)
    write(P1 + 0x17, 0)
    clearTransferLocks(false)
    stealArmed = true
  elseif scenario >= 2 and read(0xC187) == 1 then
    -- Keep P1 in the $05 jump action.  The contact itself is injected again
    -- at $2B6C so animation advancement cannot make the boundary flaky.
    placePlayer(P1, 0x05, 0x40, 0x60, 0x70, 0x01, true)
    write(0xFFCF, 0)
  end
end

local function onSteal()
  if scenario ~= 1 then return end
  stealReached = true
  print(string.format(
    "DEFENSE steal entry owner=%02X victim_action=%02X facing=%02X/%02X",
    read(0xFFCF), read(P2), read(P1 + 0x10), read(P2 + 0x10)))
end

local function onJumpContact()
  if stopping or scenario < 2 then return end
  local base = read(0xC187) == 1 and P1 or P2
  placePlayer(base, 0x05, 0x40, 0x60, 0x70, 0x01, true)
  placeBall(0x48, 0x6E, 0x10)
  write(0xFFCF, 0)
  clearTransferLocks(scenario == 2)
end

local function markStealStage(name)
  return function()
    if scenario == 1 then
      if name == "contact" then
        placePlayer(P1, 0x07, 0x40, 0x60, 0x70, 0x01, true)
        placePlayer(P2, 0x00, 0x40, 0x60, 0x70, 0x02, false)
        placeBall(0x48, 0x6E, 0)
      end
      print(string.format(
        "DEFENSE steal stage=%s player=%02X owner=%02X ball=%02X/%02X p1=%02X/%02X/%02X p2_action=%02X",
        name, read(0xC187), read(0xFFCF), read(0xC0A3), read(0xC0A7),
        read(P1 + 0x06), read(P1 + 0x15), read(P1 + 0x10), read(P2)))
    end
  end
end

local function onTransferGate()
  if stopping then return end
  print(string.format(
    "DEFENSE transfer entry scenario=%d player=%02X owner=%02X score=%02X transition=%02X first_contact=%02X cooldown=%02X",
    scenario, read(0xC187), read(0xFFCF), read(0xFFE2), read(0xFFE7),
    read(0xFFF8), read(0xC12D)))
  if scenario == 2 then
    liveJumpReached = true
    print(string.format(
      "DEFENSE live-shot gate owner=%02X first_contact=%02X",
      read(0xFFCF), read(0xFFF8)))
  elseif scenario == 3 then
    reboundJumpReached = true
    reboundPlayer = read(0xC187)
    print(string.format(
      "DEFENSE post-contact gate owner=%02X first_contact=%02X",
      read(0xFFCF), read(0xFFF8)))
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
    input.b = scenario == 1 and stealArmed
  end
  emu.setInput(input, 0)
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onEndFrame()
  if stopping then return end
  if gameplayReached then
    if scenario == 1 and stealReached then
      expect(read(0xFFCF) == 1,
        "$2B14 did not transfer deterministic possession to P1")
      print(string.format("DEFENSE steal result owner=%02X latch=%02X",
        read(0xFFCF), read(P1 + 0x17)))
      scenario = 2
      gameplayFrames = 0
    elseif scenario == 2 and liveJumpReached then
      expect(read(0xFFCF) == 0,
        "$FFF8 did not block a live-shot jump possession award")
      scenario = 3
      gameplayFrames = 0
    elseif scenario == 3 and reboundJumpReached then
      expect(read(0xFFCF) == reboundPlayer,
        "$2B6C did not award the same contact after first contact")
      if not stopping then
        stopping = true
        print(failures == 0 and
          "TRACE PASSED: $2B14 steal and $2B6C/$2B88 live-shot lock" or
          string.format("TRACE FAILED: %d mismatch(es)", failures))
        emu.stop(failures == 0 and 0 or 3)
      end
    elseif gameplayFrames > 180 and not stopping then
      stopping = true
      print(string.format("TRACE ERROR: defense scenario %d timed out", scenario))
      emu.stop(2)
    end
  elseif totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSteal, emu.callbackType.exec,
  0x2B14, 0x2B14, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(markStealStage("contact"), emu.callbackType.exec,
  0x2B46, 0x2B46, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(markStealStage("contact-pass"), emu.callbackType.exec,
  0x2B4A, 0x2B4A, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(markStealStage("facing-pass"), emu.callbackType.exec,
  0x2B88, 0x2B88, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onJumpContact, emu.callbackType.exec,
  0x2B6C, 0x2B6C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTransferGate, emu.callbackType.exec,
  0x2B88, 0x2B88, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
