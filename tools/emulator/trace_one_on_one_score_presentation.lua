-- Headless Mesen trace for the One-on-One made-basket presentation chain.
-- Extends the proven 37-frame release through the score sound, two four-frame
-- presentation holds, 34-frame fade out, $20F7 possession reset, 34-frame
-- fade in, the final counted inbound update, and playable input restoration.

local totalFrames = 0
local gameplayFrames = 0
local gameplayReached = false
local rosterChooseCount = 0
local opponentMoved = false
local mem = emu.memType.gameboyDebug
local scoreFrame = nil
local scoreCommitFrame = nil
local fadeOutFrame = nil
local resetFrame = nil
local fadeInFrame = nil
local inboundFrame = nil
local playableInboundFrame = nil
local soundCommand = nil
local soundCommands = {}
local fadeValues = {}
local fadeLcdcValues = {}
local failures = 0
local stopping = false
local scoreBallFirstBounce = false
local scoreBallSecondBounce = false
local scoreBallThirdBounce = false

local function read(address)
  return emu.read(address, mem, false)
end

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE ERROR: " .. message)
  end
end

local function onPlayerUpdate()
  if not gameplayReached then
    gameplayReached = true
    gameplayFrames = 0
  elseif resetFrame ~= nil and fadeInFrame ~= nil then
    if inboundFrame == nil then
      inboundFrame = totalFrames
      print(string.format(
        "INBOUND_WAIT frame=%d delta=%d owner=%02X wait=%02X lcdc=%02X",
        inboundFrame, inboundFrame - scoreFrame, read(0xFFCF), read(0xFFEB),
        read(0xFF40)))
    end
    if read(0xFFEB) == 0 and playableInboundFrame == nil then
      playableInboundFrame = totalFrames
      print(string.format(
        "INBOUND_PLAY frame=%d delta=%d owner=%02X p1_action=%02X p2_action=%02X bgp=%02X lcdc=%02X",
        playableInboundFrame, playableInboundFrame - scoreFrame,
        read(0xFFCF), read(0xFF9D), read(0xFFB6), read(0xFF47),
        read(0xFF40)))
    end
  end
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
end

local function onScore()
  if scoreFrame == nil then
    scoreFrame = totalFrames
    print(string.format(
      "SCORE frame=%d xyz=%02X,%02X,%02X owner=%02X points_flag=%02X",
      scoreFrame, read(0xC0A3), read(0xC0A7), read(0xC0AB),
      read(0xFFCF), read(0xFFD7)))
    print(string.format("SCORE_STATE phase=%02X mode_gate=%02X",
      read(0xFFB0), read(0xFFC9)))
  end
end

local function onSound()
  if scoreFrame ~= nil then
    print(string.format("SOUND_ENTRY frame=%d active=%02X pointer=%02X%02X",
      totalFrames, read(0xC193), read(0xC194), read(0xDD72)))
  end
end

local function onSoundSelected()
  if scoreFrame ~= nil then
    local command = read(0xC193)
    table.insert(soundCommands, command)
    if command == 0x05 then soundCommand = command end
    print(string.format("SOUND frame=%d command=%02X pointer=%02X%02X",
      totalFrames, command, read(0xC194), read(0xDD72)))
  end
end

local function onFadeOut()
  if scoreFrame ~= nil and fadeOutFrame == nil then
    fadeOutFrame = totalFrames
    print(string.format("FADE_OUT frame=%d delta=%d bgp=%02X",
      fadeOutFrame, fadeOutFrame - scoreFrame, read(0xFF47)))
  end
end

local function onFadeStep()
  if scoreFrame ~= nil then
    local value = read(0xFF47)
    table.insert(fadeValues, value)
    table.insert(fadeLcdcValues, read(0xFF40))
    print(string.format(
      "FADE_STEP frame=%d stage=%d bgp=%02X obj_flag=%02X lcdc=%02X obp=%02X/%02X",
      totalFrames, #fadeValues, value, read(0xC12F), read(0xFF40),
      read(0xFF48), read(0xFF49)))
  end
end

local function onPossessionReset()
  if fadeOutFrame ~= nil and resetFrame == nil then
    resetFrame = totalFrames
    print(string.format(
      "RESET frame=%d delta=%d old_owner=%02X next_side=%02X bgp=%02X",
      resetFrame, resetFrame - scoreFrame, read(0xFFCF), read(0xFFD0),
      read(0xFF47)))
  end
end

local function onFadeIn()
  if resetFrame ~= nil and fadeInFrame == nil then
    fadeInFrame = totalFrames
    print(string.format("FADE_IN frame=%d delta=%d bgp=%02X",
      fadeInFrame, fadeInFrame - scoreFrame, read(0xFF47)))
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
    input.a = gameplayFrames == 10 or gameplayFrames == 47
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if scoreFrame ~= nil and totalFrames - scoreFrame <= 190 then
    local delta = totalFrames - scoreFrame
    if delta == 76 and read(0xC0AA) == 0 and read(0xC0AB) == 0 and
       read(0xC0A8) == 0x53 and read(0xC0A9) == 0x01 then
      scoreBallFirstBounce = true
    elseif delta == 121 and read(0xC0AA) == 0 and read(0xC0AB) == 0 and
       read(0xC0A8) == 0x17 and read(0xC0A9) == 0x01 then
      scoreBallSecondBounce = true
    elseif delta == 158 and read(0xC0AA) == 0 and read(0xC0AB) == 0 and
       read(0xC0A8) == 0xDB and read(0xC0A9) == 0x00 then
      scoreBallThirdBounce = true
    end
    print(string.format(
      "SCORE_BALL delta=%d xyz=%02X,%02X,%02X raw_z=%02X%02X vz=%02X%02X lcdc=%02X",
      totalFrames - scoreFrame, read(0xC0A3), read(0xC0A7), read(0xC0AB),
      read(0xC0AB), read(0xC0AA), read(0xC0A9), read(0xC0A8),
      read(0xFF40)))
  end
  if scoreFrame ~= nil and scoreCommitFrame == nil and read(0xC133) ~= 0 then
    scoreCommitFrame = totalFrames
    print(string.format("SCORE_COMMIT frame=%d delta=%d p1_bcd=%02X sound=%02X",
      scoreCommitFrame, scoreCommitFrame - scoreFrame, read(0xC133),
      read(0xC193)))
  end
  if playableInboundFrame ~= nil and
      totalFrames >= playableInboundFrame + 2 and not stopping then
    stopping = true
    expect(soundCommand == 0x05, "$1F23 did not dispatch score sound command $05")
    expect(scoreCommitFrame ~= nil and scoreCommitFrame - scoreFrame == 65,
      "$1F23 did not commit the score 65 frames after $1E0E")
    expect(fadeOutFrame ~= nil and resetFrame ~= nil and fadeInFrame ~= nil,
      "$0C13 post-score fade/reset chain was incomplete")
    expect(#fadeValues == 8, "$27C7/$27CC did not execute eight palette stages")
    if #fadeValues == 8 then
      local expected = {0xE4, 0xF9, 0xFE, 0xFF, 0xFF, 0xFE, 0xF9, 0xE4}
      local expectedLcdc = {0x87, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85, 0x85}
      for i = 1, 8 do
        expect(fadeValues[i] == expected[i],
          string.format("fade stage %d was %02X, expected %02X",
            i, fadeValues[i], expected[i]))
        expect(fadeLcdcValues[i] == expectedLcdc[i],
          string.format("fade stage %d LCDC was %02X, expected %02X",
            i, fadeLcdcValues[i], expectedLcdc[i]))
      end
    end
    expect(resetFrame - fadeOutFrame == 34,
      "$27C7 fade-out duration was not 34 frames")
    expect(inboundFrame - scoreFrame == 254,
      "score-to-counted-inbound duration was not the traced 254 frames")
    expect(playableInboundFrame - scoreFrame == 258,
      "score-to-playable-inbound duration was not the traced 258 frames")
    expect(read(0xFF47) == 0xE4, "$27CC did not restore BGP=$E4")
    expect(read(0xFF40) == 0x87, "$2814 did not restore LCDC OBJ display")
    expect(scoreBallFirstBounce and scoreBallSecondBounce and
           scoreBallThirdBounce,
      "$1E0E/$7BE8/$1E77 score-ball bounce sequence did not match")
    print(failures == 0 and
      "TRACE PASSED: $1E0E/$2F88/$0C13/$27C7/$20F7/$27CC score presentation" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif not gameplayReached and totalFrames >= 3600 and not stopping then
    stopping = true
    print("TRACE ERROR: One-on-One gameplay was not reached")
    emu.stop(2)
  elseif gameplayReached and gameplayFrames >= 900 and inboundFrame == nil and not stopping then
    stopping = true
    print("TRACE ERROR: score presentation did not return to inbound gameplay")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onPlayerUpdate, emu.callbackType.exec,
  0x702D, 0x702D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onScore, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSound, emu.callbackType.exec,
  0x2F88, 0x2F88, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeOut, emu.callbackType.exec,
  0x27C7, 0x27C7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeIn, emu.callbackType.exec,
  0x27CC, 0x27CC, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeStep, emu.callbackType.exec,
  0x2820, 0x2820, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeStep, emu.callbackType.exec,
  0x282A, 0x282A, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeStep, emu.callbackType.exec,
  0x282F, 0x282F, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onFadeStep, emu.callbackType.exec,
  0x2834, 0x2834, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onPossessionReset, emu.callbackType.exec,
  0x20F7, 0x20F7, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
