-- Headless proof for the rating-dependent Free Throw clean-make table.
-- Forces roster $00/profile 2 target $50/$3B and requires
-- $1A7E->$1A94->$1C05->$1E0E on the original cartridge.

local totalFrames = 0
local modeFrames = 0
local menuReached = false
local selectedMode = false
local modeReached = false
local launchFrame = nil
local tableEntrySeen = false
local profileTableSeen = false
local directMakeSeen = false
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

local function onModeEntry()
  modeReached = true
  modeFrames = 0
end

local function onLaunch()
  launchFrame = modeFrames
  print(string.format(
    "FT_CLEAN_LAUNCH frame=%d aim=%02X,%02X profile=%02X",
    modeFrames, read(0xC0AF), read(0xC0B3), read(0xFFB5)))
end

local function onTableEntry()
  tableEntrySeen = true
  print(string.format(
    "FT_CLEAN_TABLE pc=1A7E frame=%d aim=%02X,%02X profile=%02X",
    modeFrames, read(0xC0AF), read(0xC0B3), read(0xFFB5)))
end

local function onProfileTable()
  profileTableSeen = true
end

local function onDirectMake()
  directMakeSeen = true
  print(string.format(
    "FT_CLEAN_MATCH pc=1C05 frame=%d delta=%d aim=%02X,%02X profile=%02X",
    modeFrames, modeFrames - (launchFrame or modeFrames),
    read(0xC0AF), read(0xC0B3), read(0xFFB5)))
end

local function onMadeBasket()
  if stopping then return end
  stopping = true
  expect(launchFrame ~= nil, "$7C58 launch was not observed")
  expect(tableEntrySeen, "$1A7E clean-make X table was not reached")
  expect(profileTableSeen, "$1A94 did not select a $1AB0/$1AB8/$1ABE Y list")
  expect(directMakeSeen, "$1C05 direct-make jump was not reached")
  expect(read(0xC0AF) == 0x50 and read(0xC0B3) == 0x3B,
    "forced clean target changed before outcome dispatch")
  expect(read(0xFFB5) == 0x02, "roster $00 did not select profile 2")
  expect(modeFrames - (launchFrame or modeFrames) == 77,
    "clean-table make did not occur at release +77")
  print(failures == 0 and
    "TRACE PASSED: $1A7E->$1A94->$1C05 profile-2 clean Free Throw make" or
    string.format("TRACE FAILED: %d mismatch(es)", failures))
  emu.stop(failures == 0 and 0 or 3)
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not modeReached then
    if menuReached and not selectedMode then
      input.down = true
      selectedMode = true
    elseif totalFrames % 30 == 1 then
      input.start = true
    end
  else
    modeFrames = modeFrames + 1
    if modeFrames == 44 then
      write(0xC0AF, 0x50)
      write(0xC0B3, 0x3B)
      write(0xC0AC, 0)
      write(0xC0AD, 0)
      write(0xC0B0, 0)
      write(0xC0B1, 0)
    end
    input.a = modeFrames == 45
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if totalFrames >= 5000 and not stopping then
    stopping = true
    print("TRACE ERROR: clean Free Throw make did not complete")
    emu.stop(2)
  end
end

emu.addMemoryCallback(function() menuReached = true end,
  emu.callbackType.exec, 0x03B9, 0x03B9,
  emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onModeEntry, emu.callbackType.exec,
  0x0C8E, 0x0C8E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTableEntry, emu.callbackType.exec,
  0x1A7E, 0x1A7E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onProfileTable, emu.callbackType.exec,
  0x1A94, 0x1A94, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onDirectMake, emu.callbackType.exec,
  0x1C05, 0x1C05, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMadeBasket, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
