-- Headless Mesen trace for One-on-One presentation/audio parity.
-- Captures the roster-selection command sequence, final $6F2A held-ball
-- placement/dribble cue, and $1ECC net tile sequence around a made basket.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local scoreFrame = nil
local netSteps = {}
local rosterSounds = {}
local gameplaySounds = {}
local dribbleSamples = {}
local failures = 0
local stopping = false
local mem = emu.memType.gameboyDebug

local function read(address)
  return emu.read(address, mem, false)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function hexBytes(address, count)
  local values = {}
  for i = 0, count - 1 do
    table.insert(values, string.format("%02X", read(address + i)))
  end
  return table.concat(values, " ")
end

local function netTilemap()
  local values = {}
  for row = 0, 2 do
    table.insert(values, string.format("%02X %02X",
      read(0x9849 + row * 0x20), read(0x984A + row * 0x20)))
  end
  return table.concat(values, " / ")
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
  print(string.format("ROSTER_CHOOSE frame=%d count=%d entry=%02X",
    totalFrames, rosterChooseCount, read(0xFFF6)))
end

local function onSoundSelected()
  local command = read(0xC193)
  if not gameplayReached then
    table.insert(rosterSounds, command)
    print(string.format("ROSTER_SOUND frame=%d command=%02X active=%02X",
      totalFrames, command, read(0xC194)))
  else
    table.insert(gameplaySounds, command)
    print(string.format("GAME_SOUND frame=%d game=%d command=%02X active=%02X",
      totalFrames, gameplayFrames, command, read(0xC194)))
  end
end

local function onPlayerUpdate()
  if not gameplayReached then
    gameplayReached = true
    gameplayFrames = 0
    print(string.format("GAMEPLAY frame=%d owner=%02X", totalFrames, read(0xFFCF)))
  end
end

local function onHeldBallFinal()
  local owner = read(0xFFCF)
  local base = owner == 1 and 0xFF9D or 0xFFB6
  local action = read(base)
  local record = read(base + 3)
  if #dribbleSamples < 32 then
    table.insert(dribbleSamples, {
      frame = gameplayFrames, action = action, record = record,
      x = read(0xC0A3), y = read(0xC0A7), z = read(0xC0AB),
      px = read(base + 6), py = read(base + 5), flags = read(base + 2)
    })
    print(string.format(
      "HELD frame=%d action=%02X record=%02X player=%02X,%02X flags=%02X ball=%02X,%02X,%02X",
      gameplayFrames, action, record, read(base + 6), read(base + 5),
      read(base + 2), read(0xC0A3), read(0xC0A7), read(0xC0AB)))
  end
end

local function onScore()
  if scoreFrame == nil then
    scoreFrame = totalFrames
    print(string.format("SCORE frame=%d net=%s", scoreFrame, netTilemap()))
  end
end

local function onNetWrite()
  if scoreFrame ~= nil then
    local step = {
      delta = totalFrames - scoreFrame,
      counter = read(0xC129), timer = read(0xC168), tiles = netTilemap()
    }
    table.insert(netSteps, step)
    print(string.format("NET frame=%d delta=%d state=%02X timer=%02X tiles=%s",
      totalFrames, step.delta, step.counter, step.timer, step.tiles))
    if #netSteps == 1 then
      for tile = 0x61, 0x70 do
        print(string.format("NET_TILE id=%02X bytes=%s", tile,
          -- LCDC=$87 selects signed BG tile addressing, so IDs $61..$70
          -- resolve from the $9000 court stream, not OBJ VRAM at $8000.
          hexBytes(0x9000 + tile * 16, 16)))
      end
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
    input.a = gameplayFrames == 80 or gameplayFrames == 117
  end
  emu.setInput(input, 0)
end

local function contains(values, wanted)
  for _, value in ipairs(values) do
    if value == wanted then return true end
  end
  return false
end

local function onEndFrame()
  if scoreFrame ~= nil and totalFrames >= scoreFrame + 70 and not stopping then
    stopping = true
    expect(#netSteps == 4, "$1ECC did not produce four post-score net writes")
    if #netSteps == 4 then
      local expectedDelta = {20, 35, 50, 65}
      local expectedTiles = {
        "61 62 / 67 68 / 69 6A",
        "6B 6C / 6D 6E / 6F 70",
        "61 62 / 67 68 / 69 6A",
        "61 62 / 63 64 / 65 66"
      }
      for i = 1, 4 do
        expect(netSteps[i].delta == expectedDelta[i],
          string.format("net step %d delta mismatch", i))
        expect(netSteps[i].tiles == expectedTiles[i],
          string.format("net step %d tile pattern mismatch", i))
      end
    end
    expect(contains(rosterSounds, 0x0E),
      "roster navigation never selected command $0E")
    expect(contains(gameplaySounds, 0x0C),
      "held-ball record 6 never selected command $0C")
    expect(contains(gameplaySounds, 0x0D),
      "movement/action change never selected command $0D")
    expect(contains(gameplaySounds, 0x08),
      "net bend never selected command $08")
    expect(contains(gameplaySounds, 0x05),
      "score commit never selected command $05")
    print(failures == 0 and
      "TRACE PASSED: roster audio, $6F2A dribble placement, and $1ECC score net" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 4000 and not stopping then
    stopping = true
    print("TRACE ERROR: timed out before presentation trace completed")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onHeldBallFinal, emu.callbackType.exec,
  0x6FE1, 0x6FE1, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onScore, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onNetWrite, emu.callbackType.exec,
  0x1EF4, 0x1EF4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
