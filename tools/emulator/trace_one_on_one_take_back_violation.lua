-- Headless Mesen proof for the complete changed-possession shot violation:
-- bank 1 $7C58 latches owner in $C178, fixed $2C50 selects text $067C
-- (DIDN'T CLEAR BALL), $05A3 dispatches command $04, and $20F7 restarts
-- possession to the opposite player.  The fixture seeds the post-$7C58
-- latch at the reviewed $2C50 boundary so presentation timing stays live.

local totalFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local injected = false
local messageSeen = false
local commandSeen = false
local restartSeen = false
local resumeSeen = false
local violationStart = nil
local failures = 0
local stopping = false
local mem = emu.memType.gameboyDebug

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

local function onRuleDispatcher()
  gameplayReached = true
  if injected then return end
  injected = true
  write(0xFFEB, 0)
  write(0xFFCF, 1)
  write(0xFFD0, 1)
  write(0xC178, 1)
  print(string.format(
    "TAKE_BACK_LATCH frame=%d owner=%02X C178=%02X",
    totalFrames, read(0xFFCF), read(0xC178)))
end

local function onDidntClearMessage()
  if messageSeen then return end
  messageSeen = true
  violationStart = totalFrames
  print(string.format(
    "TAKE_BACK_MESSAGE frame=%d path=$2C50->$067C->$05A3 owner=%02X",
    totalFrames, read(0xFFCF)))
  expect(read(0xFFCF) == 1 and read(0xC178) == 1,
    "$2C50 did not preserve the violating owner")
end

local function onSoundSelected()
  if not messageSeen or commandSeen then return end
  commandSeen = true
  print(string.format("TAKE_BACK_SOUND command=%02X", read(0xC193)))
  expect(read(0xC193) == 0x04,
    "$05A3 did not dispatch rule-popup command $04")
end

local function onRestart()
  if not messageSeen or restartSeen then return end
  restartSeen = true
  print(string.format(
    "TAKE_BACK_RESTART frame=%d delta=%d offender=%02X owner_before=%02X",
    totalFrames, totalFrames - violationStart, read(0xFFD0), read(0xFFCF)))
end

local function onResume()
  if not restartSeen or resumeSeen or read(0xFFE3) ~= 0 then return end
  resumeSeen = true
  print(string.format(
    "TAKE_BACK_RESUME frame=%d delta=%d owner=%02X",
    totalFrames, totalFrames - violationStart, read(0xFFCF)))
  expect(read(0xFFCF) == 2,
    "$20F7 did not turn DIDN'T CLEAR BALL over to player two")
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
  if resumeSeen and totalFrames >= violationStart + 195 and not stopping then
    stopping = true
    expect(messageSeen and commandSeen and restartSeen,
      "take-back violation presentation path was incomplete")
    print(failures == 0 and
      "TRACE PASSED: $7C58/$C178->$2C50/$067C->$05A3/$20F7 take-back violation" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 4000 and not stopping then
    stopping = true
    print("TRACE ERROR: take-back violation trace timed out")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onRuleDispatcher, emu.callbackType.exec,
  0x2C50, 0x2C50, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onDidntClearMessage, emu.callbackType.exec,
  0x2C5A, 0x2C5A, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRestart, emu.callbackType.exec,
  0x20F7, 0x20F7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onResume, emu.callbackType.exec,
  0x0BD7, 0x0BD7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
