-- Headless Mesen proof for the ROM Free Throw lifecycle.
-- Enters menu mode $01, records the fixed-bank controller path, releases one
-- shot, and captures aiming, launch, result, sound, and next-attempt state.

local totalFrames = 0
local modeFrames = 0
local menuReached = false
local selectedMode = false
local modeReached = false
local shotReleased = false
local shotResolved = false
local launchChecked = false
local nextAttempt = false
local singleSelectorSeen = false
local musicStopped = false
local reticleSeen = false
local launchFrame = nil
local failures = 0
local stopping = false
local capturedCommand = 0
local captureUntil = -1
local mem = emu.memType.gameboyDebug

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function word(address)
  local value = read(address) | (read(address + 1) << 8)
  if value >= 0x8000 then value = value - 0x10000 end
  return value
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function onMenuLoop()
  menuReached = true
end

local function onModeEntry()
  modeReached = true
  modeFrames = 0
  print(string.format(
    "FT_ENTRY mode=%02X attempts=%02X/%02X selected_side=%02X",
    read(0xFF8F), read(0xFFAB), read(0xFFC4), read(0xFFE1)))
end

local function onSingleSelectorBranch()
  if read(0xFF8F) == 0x01 then
    singleSelectorSeen = true
    print(string.format(
      "FT_SELECTOR mode=%02X players=%02X branch=4018->4034 (no second player/VS)",
      read(0xFF8F), read(0xFF91)))
  end
end

local function onMusicCleared()
  if read(0xFF8F) == 0x01 then
    musicStopped = read(0xDD73) == 0
    print(string.format(
      "FT_MUSIC_STOP pc=0C92 command_DD73=%02X", read(0xDD73)))
  end
end

local function onReticleStored()
  if not reticleSeen and read(0xC09A) == 0x7F then
    reticleSeen = true
    print(string.format(
      "FT_RETICLE pc=1A30 oam_C098={y=%02X,x=%02X,tile=%02X,attr=%02X}",
      read(0xC098), read(0xC099), read(0xC09A), read(0xC09B)))
  end
end

local function onAimInit()
  -- Values are printed at $1931, after the four-row RNG table has loaded.
end

local function onAimInitReturn()
  print(string.format(
    "FT_AIM_INIT rng=%02X target=%02X,%02X velocity=%d,%d owner=%02X attempts=%02X/%02X",
    read(0xFFFB), read(0xC0AF), read(0xC0B3),
    word(0xC0AC), word(0xC0B0), read(0xFFCF),
    read(0xFFAB), read(0xFFC4)))
end

local function onLaunch()
  shotReleased = true
  launchFrame = modeFrames
  print(string.format(
    "FT_LAUNCH frame=%d aim=%02X,%02X ball=%02X,%02X,%02X velocity=%d,%d,%d rng=%02X",
    modeFrames, read(0xC0AF), read(0xC0B3),
    read(0xC0A3), read(0xC0A7), read(0xC0AB),
    word(0xC0A0), word(0xC0A4), word(0xC0A8), read(0xFFFB)))
end


local function onLaunchComputed()
  launchChecked = true
  print(string.format(
    "FT_VECTOR frame=%d velocity=%d,%d,%d owner=%02X first_flight=%02X",
    modeFrames, word(0xC0A0), word(0xC0A4), word(0xC0A8),
    read(0xFFCF), read(0xFFF8)))
  expect(word(0xC0A0) == -36, "$7C58 horizontal vector did not equal -36")
  expect(word(0xC0A4) == -160, "$7C58 court-depth vector did not equal -160")
  expect(word(0xC0A8) == 484, "$7C58 vertical vector did not equal 484")
end

local function onMadeBasket()
  if not shotResolved then
    shotResolved = true
    print(string.format(
      "FT_MAKE frame=%d delta=%d xyz=%02X,%02X,%02X shooter=%02X",
      modeFrames, modeFrames - (launchFrame or modeFrames),
      read(0xC0A3), read(0xC0A7), read(0xC0AB), read(0xFFD0)))
  end
end

local function collision(label)
  return function()
    print(string.format(
      "FT_COLLISION frame=%d path=%s xyz=%02X,%02X,%02X v=%d,%d,%d c164=%02X c0b5=%02X",
      modeFrames, label, read(0xC0A3), read(0xC0A7), read(0xC0AB),
      word(0xC0A0), word(0xC0A4), word(0xC0A8),
      read(0xC164), read(0xC0B5)))
  end
end

local function onAttemptPresentation()
  print(string.format(
    "FT_ATTEMPT_PRESENT frame=%d counter=%02X/%02X score=%02X/%02X result=%02X",
    modeFrames, read(0xFFAB), read(0xFFC4),
    read(0xC133), read(0xC135), read(0xFFE2)))
end

local function onSoundSelected()
  local command = read(0xC193)
  if command == 0x08 or command == 0x0A then
    capturedCommand = command
    captureUntil = modeFrames + 20
  else
    capturedCommand = 0
    captureUntil = -1
  end
  print(string.format(
    "FT_SOUND frame=%d command=%02X program=%02X priority=%02X",
    modeFrames, read(0xC193), read(0xDD72), read(0xC194)))
end

local function onApuWrite(address, value)
  if capturedCommand ~= 0 and modeFrames <= captureUntil then
    print(string.format("FT_APU frame=%d command=%02X address=%04X value=%02X",
      modeFrames, capturedCommand, address, value))
  end
end

local function onNextAttempt()
  if shotReleased then
    nextAttempt = true
    print(string.format(
      "FT_NEXT frame=%d counter=%02X/%02X score=%02X/%02X aim=%02X,%02X",
      modeFrames, read(0xFFAB), read(0xFFC4),
      read(0xC133), read(0xC135), read(0xC0AF), read(0xC0B3)))
  end
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }

  if not modeReached then
    if menuReached and not selectedMode then
      -- The menu's own $03FD path increments $FF8F from One-on-One to Free Throws.
      input.down = true
      selectedMode = true
    elseif totalFrames % 30 == 1 then
      input.start = true
    end
  else
    modeFrames = modeFrames + 1
    -- Force the ROM's exact center target for a deterministic made-shot trace.
    if modeFrames == 44 then
      write(0xC0AF, 0x52)
      write(0xC0B3, 0x3C)
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
  if nextAttempt and modeFrames >= 280 and not stopping then
    stopping = true
    expect(read(0xFF8F) == 1, "mode dispatcher did not retain Free Throw ID $01")
    expect(shotReleased, "$1CAA->$7C58 did not release the shot")
    expect(launchChecked, "$7C58 did not reach the computed-vector handoff")
    expect(shotResolved, "center target did not reach the ROM made-basket cell")
    expect(singleSelectorSeen,
      "bank-2 mode-$01 single-player selector branch was not observed")
    expect(musicStopped, "$0C8E did not clear the music command at $DD73")
    expect(reticleSeen, "$1A25 did not update OBJ reticle tile $7F")
    expect(read(0xFFAB) == 0x04 or read(0xFFC4) == 0x04,
      "$0CD1 did not decrement the selected BCD attempt counter")
    print(failures == 0 and
      "TRACE PASSED: $0C8E/$1082/$18E7/$1942/$1986/$1CAA/$7C58 Free Throw lifecycle" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 5000 and not stopping then
    stopping = true
    print("TRACE ERROR: Free Throw mode did not complete one attempt")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onMenuLoop, emu.callbackType.exec,
  0x03B9, 0x03B9, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onModeEntry, emu.callbackType.exec,
  0x0C8E, 0x0C8E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSingleSelectorBranch, emu.callbackType.exec,
  0x4018, 0x4018, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMusicCleared, emu.callbackType.exec,
  0x0C92, 0x0C92, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onReticleStored, emu.callbackType.exec,
  0x1A30, 0x1A30, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAimInit, emu.callbackType.exec,
  0x18E7, 0x18E7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAimInitReturn, emu.callbackType.exec,
  0x1931, 0x1931, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunchComputed, emu.callbackType.exec,
  0x7F28, 0x7F28, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMadeBasket, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
for address, label in pairs({
  [0x1B3F]="1B3F", [0x1B45]="1B45", [0x1B53]="1B53",
  [0x1B59]="1B59", [0x1B93]="1B93", [0x1B99]="1B99",
  [0x1BA7]="1BA7", [0x1BAD]="1BAD", [0x1BBD]="1BBD",
  [0x1C05]="1C05(make)", [0x1E74]="1E74(rim)"
}) do
  emu.addMemoryCallback(collision(label), emu.callbackType.exec,
    address, address, emu.cpuType.gameboy, emu.memType.gameboyMemory)
end
emu.addMemoryCallback(onAttemptPresentation, emu.callbackType.exec,
  0x17E2, 0x17E2, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onNextAttempt, emu.callbackType.exec,
  0x17AA, 0x17AA, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
