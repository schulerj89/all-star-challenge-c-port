-- Headless Mesen assertions for bank-1 $6A8C animation dispatch.
-- The route and controlled inputs identify action semantics while reads of
-- player +$00..+$04 verify action, display-frame, flags, record, and timer.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local mem = emu.memType.gameboyDebug
local rosterChooseCount = 0
local opponentMoved = false
local stopping = false
local failures = 0
local lastAction = -1
local observed = {
  idleBall = false, rightBall = false, downBall = false, leftBall = false,
  upBall = false,
  idleNoBall = false, rightNoBall = false, downNoBall = false,
  leftNoBall = false, upNoBall = false, directionalIdleNoBall = false,
  jump = false, steal = false,
  sixFrameCadence = false
}
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

local function scenarioName(frame)
  if frame <= 30 then return "idle-ball" end
  if frame <= 70 then return "right-ball" end
  if frame <= 110 then return "down-ball" end
  if frame <= 150 then return "left-ball" end
  if frame <= 190 then return "up-ball" end
  if frame <= 225 then return "idle-no-ball" end
  if frame <= 260 then return "right-no-ball" end
  if frame <= 295 then return "down-no-ball" end
  if frame <= 330 then return "left-no-ball" end
  if frame <= 365 then return "up-no-ball" end
  if frame <= 400 then return "directional-idle-no-ball" end
  if frame <= 480 then return "jump-no-ball" end
  return "steal-no-ball"
end

local function onAnimationDispatcher()
  gameplayReached = true
  if read(0xC187) ~= 1 then return end
  local action = read(P1)
  local display = read(P1 + 1)
  local record = read(P1 + 3)
  local timer = read(P1 + 4)
  if action ~= lastAction then
    lastAction = action
    print(string.format(
      "ANIM scenario=%s frame=%d action=%02X display=%02X record=%02X timer=%02X",
      scenarioName(gameplayFrames), gameplayFrames, action, display,
      record, timer))
  end

  if gameplayFrames <= 30 and action == 0x13 then
    observed.idleBall = true
  elseif gameplayFrames >= 31 and gameplayFrames <= 70 and action == 0x10 then
    observed.rightBall = true
    if display == 0 and record == 2 and timer == 6 then
      observed.sixFrameCadence = true
    end
  elseif gameplayFrames >= 71 and gameplayFrames <= 110 and action == 0x01 then
    observed.downBall = true
  elseif gameplayFrames >= 111 and gameplayFrames <= 150 and action == 0x10 then
    observed.leftBall = true
  elseif gameplayFrames >= 151 and gameplayFrames <= 190 and action == 0x08 then
    observed.upBall = true
  elseif gameplayFrames >= 191 and gameplayFrames <= 225 and action == 0x0D then
    observed.idleNoBall = true
  elseif gameplayFrames >= 226 and gameplayFrames <= 260 and action == 0x11 then
    observed.rightNoBall = true
  elseif gameplayFrames >= 261 and gameplayFrames <= 295 and action == 0x02 then
    observed.downNoBall = true
  elseif gameplayFrames >= 296 and gameplayFrames <= 330 and action == 0x11 then
    observed.leftNoBall = true
  elseif gameplayFrames >= 331 and gameplayFrames <= 365 and action == 0x09 then
    observed.upNoBall = true
  elseif gameplayFrames >= 366 and gameplayFrames <= 400 and action == 0x0D then
    observed.directionalIdleNoBall = true
  elseif gameplayFrames >= 401 and gameplayFrames <= 480 and action == 0x0C then
    observed.jump = true
  elseif gameplayFrames >= 481 and action == 0x0F then
    observed.steal = true
  end
end

local function onPlayerInputUpdate()
  gameplayReached = true
  if gameplayFrames == 191 then
    write(0xFFCF, 2)
    write(P1 + 0x09, 1)
    write(P2 + 0x09, 0)
  end
  if gameplayFrames >= 481 then
    -- Re-establish the reviewed $2B14 contact after the 72-frame jump.
    write(0xFFCF, 2)
    write(P1 + 0x06, 0x40)
    write(P1 + 0x15, 0x70)
    write(P1 + 0x10, 0x01)
    write(P1 + 0x09, 1)
    write(P2 + 0x00, 0x00)
    write(P2 + 0x06, 0x40)
    write(P2 + 0x15, 0x70)
    write(P2 + 0x10, 0x02)
    write(P2 + 0x09, 0)
    write(0xC0A3, 0x48)
    write(0xC0A7, 0x6E)
    write(P1 + 0x17, 0)
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
    if gameplayFrames >= 31 and gameplayFrames <= 70 then input.right = true end
    if gameplayFrames >= 71 and gameplayFrames <= 110 then input.down = true end
    if gameplayFrames >= 111 and gameplayFrames <= 150 then input.left = true end
    if gameplayFrames >= 151 and gameplayFrames <= 190 then input.up = true end
    if gameplayFrames >= 226 and gameplayFrames <= 260 then input.right = true end
    if gameplayFrames >= 261 and gameplayFrames <= 295 then input.down = true end
    if gameplayFrames >= 296 and gameplayFrames <= 330 then input.left = true end
    if gameplayFrames >= 331 and gameplayFrames <= 365 then input.up = true end
    if gameplayFrames == 406 then input.a = true end
    if gameplayFrames >= 481 and gameplayFrames <= 515 then input.b = true end
  end
  emu.setInput(input, 0)
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onEndFrame()
  if gameplayReached and gameplayFrames >= 520 and not stopping then
    stopping = true
    expect(observed.idleBall, "idle-with-ball action $13 was not observed")
    expect(observed.rightBall, "right-with-ball action $10 was not observed")
    expect(observed.downBall, "down-with-ball action $01 was not observed")
    expect(observed.leftBall, "left-with-ball action $10 was not observed")
    expect(observed.upBall, "up-with-ball action $08 was not observed")
    expect(observed.idleNoBall, "idle-without-ball action $0D was not observed")
    expect(observed.rightNoBall, "right-without-ball action $11 was not observed")
    expect(observed.downNoBall, "down-without-ball action $02 was not observed")
    expect(observed.leftNoBall, "left-without-ball action $11 was not observed")
    expect(observed.upNoBall, "up-without-ball action $09 was not observed")
    expect(observed.directionalIdleNoBall,
      "up-facing idle-without-ball action $0D was not observed")
    expect(observed.jump, "middle-family defensive jump action $0C was not observed")
    expect(observed.steal, "middle-family post-jump steal action $0F was not observed")
    expect(observed.sixFrameCadence,
      "$6A8C did not reload the second $10 record with six frames")
    print(failures == 0 and
      "TRACE PASSED: $782E/$6A8C directional actions and record cadence" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onAnimationDispatcher, emu.callbackType.exec,
  0x6A8C, 0x6A8C, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPlayerInputUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
