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
local rightMovePending = false
local rightMoveBeforeX = 0
local rightMoveBeforeAction = 0
local dispatcherCount = 0
local movementBoundaryCount = 0
local rightEntryCount = 0
local failures = 0
local mem = emu.memType.gameboyDebug
local P1 = 0xFF9D
local P2 = 0xFFB6
-- The human-controlled player is the second player block in this menu route;
-- $C187=2 selects it as the active player and $0773 returns P1 as its peer.
local ACTOR = P2
local OTHER = P1
local observed = {
  blocked = false, away = false, separated = false,
  blockedMove = false, awayMove = false, separatedMove = false
}

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
  write(ACTOR + 0x06, 0x40)
  write(ACTOR + 0x15, 0x70)
  write(ACTOR + 0x0C, 0)
  if gameplayFrames <= 90 then
    write(OTHER + 0x06, 0x48)
    write(OTHER + 0x15, 0x70)
  elseif gameplayFrames <= 150 then
    write(OTHER + 0x06, 0x3C)
    write(OTHER + 0x15, 0x70)
  else
    write(OTHER + 0x06, 0x48)
    write(OTHER + 0x15, 0x80)
  end
  write(0xC16B, 0)
  injecting = false
end

local function onCollisionWrite()
  if injecting or not gameplayReached or read(0xC187) ~= 2 then return end
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

-- $6B72 calls $6BAD only at a normal $6A8C record boundary.  The return
-- address is $6B81, so this pair observes the callback's complete side
-- effects without guessing from a later video frame.
local function onRightMoveEnter()
  if not gameplayReached then return end
  rightEntryCount = rightEntryCount + 1
  if read(0xC187) ~= 2 then return end
  rightMovePending = true
  rightMoveBeforeX = read(ACTOR + 0x06)
  rightMoveBeforeAction = read(ACTOR + 0x03)
end

local function onAnimationDispatcher()
  if gameplayReached then dispatcherCount = dispatcherCount + 1 end
end

local function onMovementBoundary()
  if gameplayReached then
    movementBoundaryCount = movementBoundaryCount + 1
  end
end

local function onRightMoveReturn()
  if not rightMovePending then return end
  rightMovePending = false
  local afterX = read(ACTOR + 0x06)
  local afterAction = read(ACTOR + 0x03)
  local delta = (afterX - rightMoveBeforeX) & 0xFF
  expect(afterAction == rightMoveBeforeAction,
    "$6BAD changed the player's action on ordinary body contact")
  local firstObservation = false
  if gameplayFrames >= 31 and gameplayFrames <= 90 then
    expect(delta == 0, "$6BAD displaced an overlapping player")
    firstObservation = not observed.blockedMove and delta == 0
    observed.blockedMove = observed.blockedMove or delta == 0
  elseif gameplayFrames >= 91 and gameplayFrames <= 150 then
    expect(delta == 4, "$6BAD did not move four pixels away from contact")
    firstObservation = not observed.awayMove and delta == 4
    observed.awayMove = observed.awayMove or delta == 4
  elseif gameplayFrames >= 151 then
    expect(delta == 4, "$6BAD did not move four pixels after separation")
    firstObservation = not observed.separatedMove and delta == 4
    observed.separatedMove = observed.separatedMove or delta == 4
  end
  if firstObservation then
    print(string.format(
      "MOVE scenario=%s frame=%d x=%02X->%02X action=%02X",
      scenario(), gameplayFrames, rightMoveBeforeX, afterX, afterAction))
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
    expect(observed.blockedMove,
      "$6BAD blocked-displacement callback was not observed")
    expect(observed.awayMove,
      "$6BAD four-pixel movement away was not observed")
    expect(observed.separatedMove,
      "$6BAD four-pixel separated movement was not observed")
    expect(dispatcherCount > 0, "$6A8C dispatcher was not observed")
    expect(movementBoundaryCount > 0,
      "$6A8C did not reach the $6B72 movement boundary")
    print(string.format("PATH $6A8C=%d $6B72=%d",
      dispatcherCount, movementBoundaryCount))
    print(string.format("PATH $6BAD=%d", rightEntryCount))
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
emu.addMemoryCallback(onRightMoveEnter, emu.callbackType.exec,
  0x6BAD, 0x6BAD, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRightMoveReturn, emu.callbackType.exec,
  0x6B81, 0x6B81, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onAnimationDispatcher, emu.callbackType.exec,
  0x6A8C, 0x6A8C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMovementBoundary, emu.callbackType.exec,
  0x6B72, 0x6B72, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
