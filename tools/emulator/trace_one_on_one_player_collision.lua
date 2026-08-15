-- Headless Mesen assertions for bank-1 $6E3C/$6EC0/$6EEA player collision.
-- Controlled rightward input proves overlap blocks motion, while the same
-- primary-axis overlap permits motion away and perpendicular separation.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local stopping = false
local injecting = false
local collisionWritePending = false
local failures = 0
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local P2 = 0xFFB6
local observed = {blocked = false, away = false, separated = false}

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

local function scenario()
  if gameplayFrames <= 30 then return "warmup" end
  if gameplayFrames <= 90 then return "blocked-right" end
  if gameplayFrames <= 150 then return "away-right" end
  return "separated-right"
end

local function onPlayerInputUpdate()
  gameplayReached = true
  if gameplayFrames <= 30 then return end
  injecting = true
  write(P1 + 0x06, 0x40)
  write(P1 + 0x15, 0x70)
  write(P1 + 0x0C, 0)
  if gameplayFrames <= 90 then
    write(P2 + 0x06, 0x48)
    write(P2 + 0x15, 0x70)
  elseif gameplayFrames <= 150 then
    write(P2 + 0x06, 0x3C)
    write(P2 + 0x15, 0x70)
  else
    write(P2 + 0x06, 0x48)
    write(P2 + 0x15, 0x80)
  end
  write(0xC16B, 0)
  injecting = false
end

local function onCollisionWrite()
  if injecting or not gameplayReached or read(0xC187) ~= 1 then return end
  collisionWritePending = true
end

local function onCollisionReturn()
  if not collisionWritePending then return end
  collisionWritePending = false
  local value = read(0xC16B)
  if gameplayFrames >= 31 and gameplayFrames <= 90 and value == 1 then
    if not observed.blocked then
      print(string.format("COLLISION scenario=%s frame=%d result=%02X",
        scenario(), gameplayFrames, value))
    end
    observed.blocked = true
  elseif gameplayFrames >= 91 and gameplayFrames <= 150 and value == 0 then
    if not observed.away then
      print(string.format("COLLISION scenario=%s frame=%d result=%02X",
        scenario(), gameplayFrames, value))
    end
    observed.away = true
  elseif gameplayFrames >= 151 and value == 0 then
    if not observed.separated then
      print(string.format("COLLISION scenario=%s frame=%d result=%02X",
        scenario(), gameplayFrames, value))
    end
    observed.separated = true
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
    if gameplayFrames >= 31 then input.right = true end
  end
  emu.setInput(input, 0)
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onEndFrame()
  if gameplayReached and gameplayFrames >= 215 and not stopping then
    stopping = true
    expect(observed.blocked, "$6E3C did not block rightward overlap")
    expect(observed.away, "$6E3C did not allow rightward motion away")
    expect(observed.separated,
      "$6E3C did not allow motion with vertical separation")
    print(failures == 0 and
      "TRACE PASSED: $6E3C directional player-pair collision" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCollisionWrite, emu.callbackType.write,
  0xC16B, 0xC16B, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCollisionReturn, emu.callbackType.exec,
  0x6F27, 0x6F27, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
