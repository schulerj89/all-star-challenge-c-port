-- Headless Mesen proof for charging -> $05A3 command $04 -> $0C49's
-- 120-frame message/fade/restart path. The fixture injects the exact prior
-- update contact latch/counter at $2C50, then lets the cartridge run freely.

local totalFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local injected = false
local chargingSeen = false
local commandSeen = false
local programSeen = false
local foulLoopSeen = false
local restartSeen = false
local resumeSeen = false
local lastBg = nil
local lastLcdc = nil
local foulStartFrame = nil
local restartFrame = nil
local failures = 0
local stopping = false
local mem = emu.memType.gameboyDebug
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
  if injected then return end
  injected = true
  write(0xFFEB, 0)
  write(0xC178, 0)
  write(0xFFCF, 1)
  place(P1, 0x01, 0x40, 0x60, 0, 1, 1)
  place(P2, 0x09, 0x4C, 0x60, 0, 0, 0x19)
end

local function onChargingMessage()
  if chargingSeen then return end
  chargingSeen = true
  foulStartFrame = totalFrames
  print(string.format(
    "FOUL charging frame=%d owner=%02X offender=%02X message_ptr=065A",
    totalFrames, read(0xFFCF), read(0xFFD0)))
  expect(read(0xFFCF) == 1 and read(0xFFD0) == 1,
    "charging did not retain the owner as offender")
end

local function onSoundSelected()
  if not chargingSeen or commandSeen then return end
  commandSeen = true
  print(string.format("FOUL_SOUND command=%02X active=%02X",
    read(0xC193), read(0xC194)))
  expect(read(0xC193) == 0x04,
    "$05A3 did not dispatch command $04")
end

local function onSoundReturn()
  if not commandSeen or programSeen then return end
  programSeen = true
  print(string.format("FOUL_PROGRAM program=%02X priority_frames=%02X",
    read(0xDD72), read(0xC194)))
  expect(read(0xDD72) == 0x8A and read(0xC194) == 0x1E,
    "command $04 did not select active program $8A/priority $1E")
end

local function onFoulLoop()
  if not chargingSeen or foulLoopSeen then return end
  foulLoopSeen = true
  print(string.format("FOUL_LOOP frame=%d FFE3=%02X", totalFrames,
    read(0xFFE3)))
  expect(read(0xFFE3) ~= 0, "$0C49 entered without the foul event latch")
end

local function onRestart()
  if not foulLoopSeen or restartSeen then return end
  restartSeen = true
  restartFrame = totalFrames
  print(string.format(
    "FOUL_RESTART frame=%d delta=%d offender=%02X owner_before=%02X",
    totalFrames, totalFrames - foulStartFrame, read(0xFFD0), read(0xFFCF)))
end

local function onResume()
  if not restartSeen or resumeSeen or read(0xFFE3) ~= 0 then return end
  resumeSeen = true
  print(string.format(
    "FOUL_RESUME frame=%d delta=%d owner=%02X p1_y=%02X p2_y=%02X",
    totalFrames, totalFrames - foulStartFrame, read(0xFFCF),
    read(P1 + 0x15), read(P2 + 0x15)))
  expect(read(0xFFCF) == 2,
    "$20F7 did not restart to the player opposite the charging offender")
end

local function onPresentationRegister(address, value)
  if not chargingSeen then return end
  if address == 0xFF47 and value ~= lastBg then
    lastBg = value
    print(string.format("FOUL_BGP frame=%d delta=%d value=%02X",
      totalFrames, totalFrames - foulStartFrame, value))
  elseif address == 0xFF40 and value ~= lastLcdc then
    lastLcdc = value
    print(string.format("FOUL_LCDC frame=%d delta=%d value=%02X",
      totalFrames, totalFrames - foulStartFrame, value))
  end
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not gameplayReached and totalFrames % 30 == 1 then
    if rosterChooseCount >= 2 and not opponentMoved then
      input.right = true
      opponentMoved = true
    else
      input.start = true
    end
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if resumeSeen and totalFrames >= foulStartFrame + 195 and not stopping then
    stopping = true
    expect(chargingSeen and commandSeen and programSeen and foulLoopSeen and
           restartSeen, "foul presentation path was incomplete")
    print(failures == 0 and
      "TRACE PASSED: $2CCA->$05A3 command $04->$0C49->$20F7 foul flow" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 4000 and not stopping then
    stopping = true
    print("TRACE ERROR: foul presentation trace timed out")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onRuleDispatcher, emu.callbackType.exec,
  0x2C50, 0x2C50, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onChargingMessage, emu.callbackType.exec,
  0x2CF4, 0x2CF4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundReturn, emu.callbackType.exec,
  0x2FAC, 0x2FAC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFoulLoop, emu.callbackType.exec,
  0x0C49, 0x0C49, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRestart, emu.callbackType.exec,
  0x20F7, 0x20F7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onResume, emu.callbackType.exec,
  0x0BD7, 0x0BD7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPresentationRegister, emu.callbackType.write,
  0xFF40, 0xFF47, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
