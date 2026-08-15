-- Headless Mesen trace for One-on-One presentation/audio parity.
-- Captures the roster-selection command sequence, final $6F2A held-ball
-- placement/dribble cue, exact $05/$0D APU writes, $1ECC net sequence, and
-- $20F7/$21C8/$21E1 post-score take-out placement.

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
local pendingSoundCommand = nil
local audioCaptures = {}
local activeAudioCapture = nil
local capturedCommands = {}
local inboundSetup = nil
local inboundSetupCount = 0
local inboundFirstUpdate = nil
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
  pendingSoundCommand = command
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

local function masterClock()
  return emu.getState()["masterClock"] or 0
end

local function onSoundSelectionReturn()
  local command = pendingSoundCommand
  if command == nil then return end
  local program = read(0xDD72)
  local priorityFrames = read(0xC194)
  print(string.format(
    "SOUND_PROGRAM command=%02X program=%02X priority_frames=%02X",
    command, program, priorityFrames))
  if (command == 0x0C or command == 0x0D or command == 0x05) and
      not capturedCommands[command] and activeAudioCapture == nil then
    activeAudioCapture = {
      command = command,
      program = program,
      priorityFrames = priorityFrames,
      startClock = masterClock(),
      startFrame = totalFrames,
      events = {}
    }
    capturedCommands[command] = true
  end
  pendingSoundCommand = nil
end

local function onApuWrite(address, value)
  if activeAudioCapture == nil then return end
  if address == 0xFF25 and activeAudioCapture.lastRoute == value then return end
  if address == 0xFF25 then activeAudioCapture.lastRoute = value end
  table.insert(activeAudioCapture.events, {
    address = address,
    value = value,
    cycles = masterClock() - activeAudioCapture.startClock,
    frame = totalFrames - activeAudioCapture.startFrame
  })
end

local function finishAudioCapture()
  if activeAudioCapture == nil then return end
  local capture = activeAudioCapture
  activeAudioCapture = nil
  audioCaptures[capture.command] = capture
  print(string.format(
    "AUDIO_CAPTURE command=%02X program=%02X priority_frames=%d events=%d",
    capture.command, capture.program, capture.priorityFrames,
    #capture.events))
  for _, event in ipairs(capture.events) do
    print(string.format(
      "APU command=%02X frame=%d cycles=%d address=%04X value=%02X",
      capture.command, event.frame, event.cycles,
      event.address, event.value))
  end
end

local function onPlayerUpdate()
  if inboundSetup ~= nil and inboundFirstUpdate == nil then
    inboundFirstUpdate = {
      owner = read(0xFFCF),
      p1GroundY = read(0xFFB2), p1TargetY = read(0xFFB3),
      p2GroundY = read(0xFFCB), p2TargetY = read(0xFFCC)
    }
    print(string.format(
      "INBOUND_FIRST_UPDATE owner=%02X p1_ground=%02X target=%02X p2_ground=%02X target=%02X",
      inboundFirstUpdate.owner, inboundFirstUpdate.p1GroundY,
      inboundFirstUpdate.p1TargetY, inboundFirstUpdate.p2GroundY,
      inboundFirstUpdate.p2TargetY))
  end
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

local function onInboundSetupComplete()
  inboundSetupCount = inboundSetupCount + 1
  inboundFirstUpdate = nil
  inboundSetup = {
    ffd0 = read(0xFFD0), owner = read(0xFFCF),
    p1 = {action = read(0xFF9D), visualY = read(0xFFA2),
          x = read(0xFFA3), groundY = read(0xFFB2), targetY = read(0xFFB3)},
    p2 = {action = read(0xFFB6), visualY = read(0xFFBB),
          x = read(0xFFBC), groundY = read(0xFFCB), targetY = read(0xFFCC)},
    ballX = read(0xC0A3), ballY = read(0xC0A7)
  }
  print(string.format(
    "INBOUND_SETUP count=%d ffd0=%02X owner=%02X p1=%02X,%02X,%02X,%02X,%02X p2=%02X,%02X,%02X,%02X,%02X ball=%02X,%02X",
    inboundSetupCount,
    inboundSetup.ffd0, inboundSetup.owner,
    inboundSetup.p1.action, inboundSetup.p1.visualY,
    inboundSetup.p1.x, inboundSetup.p1.groundY, inboundSetup.p1.targetY,
    inboundSetup.p2.action, inboundSetup.p2.visualY,
    inboundSetup.p2.x, inboundSetup.p2.groundY, inboundSetup.p2.targetY,
    inboundSetup.ballX, inboundSetup.ballY))
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

local function audioContains(capture, frame, address, value)
  if capture == nil then return false end
  for _, event in ipairs(capture.events) do
    if event.frame == frame and event.address == address and
       event.value == value then return true end
  end
  return false
end

local function onEndFrame()
  if activeAudioCapture ~= nil then
    local captureFrames = totalFrames - activeAudioCapture.startFrame
    local wantedFrames = activeAudioCapture.command == 0x05 and 110 or 25
    if captureFrames >= wantedFrames then finishAudioCapture() end
  end
  if scoreFrame ~= nil and totalFrames >= scoreFrame + 280 and not stopping then
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
    expect(audioCaptures[0x0D] ~= nil and
           audioCaptures[0x0D].program == 0x91 and
           audioCaptures[0x0D].priorityFrames == 0x14 and
           #audioCaptures[0x0D].events == 10 and
           audioContains(audioCaptures[0x0D], 1, 0xFF10, 0x1F) and
           audioContains(audioCaptures[0x0D], 1, 0xFF11, 0xF9) and
           audioContains(audioCaptures[0x0D], 1, 0xFF12, 0xF9) and
           audioContains(audioCaptures[0x0D], 1, 0xFF13, 0xBA) and
           audioContains(audioCaptures[0x0D], 1, 0xFF14, 0xFF) and
           audioContains(audioCaptures[0x0D], 2, 0xFF13, 0xBB) and
           audioContains(audioCaptures[0x0D], 3, 0xFF13, 0xBC) and
           audioContains(audioCaptures[0x0D], 4, 0xFF25, 0x00),
      "command $0D did not execute sound program $11 with APU writes")
    expect(audioCaptures[0x0C] ~= nil and
           audioCaptures[0x0C].program == 0x82 and
           audioCaptures[0x0C].priorityFrames == 0x13 and
           #audioCaptures[0x0C].events == 37 and
           audioContains(audioCaptures[0x0C], 1, 0xFF16, 0x7A) and
           audioContains(audioCaptures[0x0C], 1, 0xFF17, 0xF1) and
           audioContains(audioCaptures[0x0C], 1, 0xFF18, 0x00) and
           audioContains(audioCaptures[0x0C], 1, 0xFF19, 0x80) and
           audioContains(audioCaptures[0x0C], 6, 0xFF19, 0x80) and
           audioContains(audioCaptures[0x0C], 12, 0xFF25, 0x00),
      "command $0C did not execute sound program $02")
    expect(audioCaptures[0x05] ~= nil and
           audioCaptures[0x05].program == 0x8C and
           audioCaptures[0x05].priorityFrames == 0x64 and
           #audioCaptures[0x05].events == 177 and
           audioContains(audioCaptures[0x05], 1, 0xFF16, 0x7F) and
           audioContains(audioCaptures[0x05], 1, 0xFF17, 0xFB) and
           audioContains(audioCaptures[0x05], 1, 0xFF18, 0x63) and
           audioContains(audioCaptures[0x05], 1, 0xFF19, 0xBD) and
           audioContains(audioCaptures[0x05], 1, 0xFF10, 0x00) and
           audioContains(audioCaptures[0x05], 1, 0xFF11, 0x7A) and
           audioContains(audioCaptures[0x05], 1, 0xFF12, 0xFB) and
           audioContains(audioCaptures[0x05], 1, 0xFF13, 0x0B) and
           audioContains(audioCaptures[0x05], 17, 0xFF18, 0x0B) and
           audioContains(audioCaptures[0x05], 17, 0xFF13, 0x72) and
           audioContains(audioCaptures[0x05], 25, 0xFF18, 0x72) and
           audioContains(audioCaptures[0x05], 25, 0xFF13, 0xB2) and
           audioContains(audioCaptures[0x05], 72, 0xFF18, 0x71) and
           audioContains(audioCaptures[0x05], 72, 0xFF13, 0xB1) and
           audioContains(audioCaptures[0x05], 73, 0xFF25, 0x00),
      "command $05 did not execute sound program $0C with APU writes")
    expect(inboundSetup ~= nil, "$20F7 inbound setup was not reached")
    if inboundSetup ~= nil then
      expect(inboundSetup.owner == (inboundSetup.ffd0 == 1 and 2 or 1),
        "$20F7 did not award possession opposite $FFD0")
      local scorer = inboundSetup.ffd0 == 1 and inboundSetup.p1 or inboundSetup.p2
      local inbounder = inboundSetup.owner == 1 and inboundSetup.p1 or inboundSetup.p2
      expect(scorer.action == 0x06 and scorer.visualY == 0x60 and
             scorer.x == 0x4C and scorer.groundY == 0x00 and
             scorer.targetY == 0x88,
        "$21E1 scorer template mismatch")
      expect(inbounder.action == 0x0D and inbounder.visualY == 0x70 and
             inbounder.x == 0x4C and inbounder.groundY == 0x00 and
             inbounder.targetY == 0x98,
        "$21C8 take-out template mismatch")
      expect(inboundSetup.ballX == 0x50 and inboundSetup.ballY == 0x90,
        "$20F7 inbound ball origin mismatch")
    end
    expect(inboundSetupCount >= 2,
      "$20F7 was not observed for both match entry and post-score take-out")
    print(failures == 0 and
      "TRACE PASSED: exact $05/$0D APU, $20F7 inbound, $6F2A ball, and $1ECC net" or
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
emu.addMemoryCallback(onSoundSelectionReturn, emu.callbackType.exec,
  0x2FAC, 0x2FAC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onHeldBallFinal, emu.callbackType.exec,
  0x6FE1, 0x6FE1, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onScore, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onNetWrite, emu.callbackType.exec,
  0x1EF4, 0x1EF4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onInboundSetupComplete, emu.callbackType.exec,
  0x2169, 0x2169, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
