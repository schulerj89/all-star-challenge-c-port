-- Headless H-O-R-S-E mode trace. This script follows menu mode $02 through
-- roster selection and records the fixed/bank-1 called-shot state machine.

local totalFrames = 0
local horseFrames = 0
local menuReached = false
local menuMoves = 0
local settingsDispatcherSeen = false
local settingsBypassSeen = false
local horseReached = false
local rosterChooseCount = 0
local opponentMoved = false
local stopping = false
local firstTurnFrame = nil
local turnCount = 0
local callerSeen = false
local matchSeen = false
local saveSeen = false
local makeSeen = false
local makeFrame = nil
local net08TimingSeen = false
local score05TimingSeen = false
local missContact0ASeen = false
local netBendSeen = false
local netDeepSeen = false
local netReturnSeen = false
local netRestSeen = false
local lastNetCells = nil
local horseHudSeen = false
local letterSeen = false
local letterDrawSeen = false
local command7Seen = false
local apu7Seen = false
local capturedCommand = 0
local captureUntil = -1
local failures = 0
local mem = emu.memType.gameboyDebug

local function expect(condition, message)
  if not condition then
    failures = failures + 1
    print("TRACE MISMATCH: " .. message)
  end
end

local function read(address)
  return emu.read(address, mem, false)
end

local function write(address, value)
  emu.write(address, value, mem)
end

local function horseState(label)
  print(string.format(
    "HORSE_%s frame=%d players=%02X turn=%02X caller=%02X letters=%02X/%02X made=%02X spot=%02X,%02X p1=%02X,%02X p2=%02X,%02X",
    label, horseFrames, read(0xFF91), read(0xFFDA), read(0xC172),
    read(0xFFAB), read(0xFFC4), read(0xC180), read(0xFFDB), read(0xFFDC),
    read(0xFFA3), read(0xFFA2), read(0xFFBC), read(0xFFBB)))
end

local function bytes(address, count)
  local values = {}
  for i = 0, count - 1 do
    values[#values + 1] = string.format("%02X", read(address + i))
  end
  return table.concat(values, "")
end

local function horseBgRows(label, firstRow, rowCount)
  for row = firstRow, firstRow + rowCount - 1 do
    print(string.format("HORSE_BG_%s row=%02d data=%s", label, row,
      bytes(0x9800 + row * 0x20, 20)))
  end
end

local function onMenuLoop()
  menuReached = true
end

local function onSettingsDispatcher()
  if read(0xFF8F) == 0x02 then
    settingsDispatcherSeen = true
    print("HORSE_SETTINGS_22EF mode=02 branch=immediate-bypass")
  end
end

local function onSettingsBypass()
  if read(0xFF8F) == 0x02 then
    settingsBypassSeen = true
    print("HORSE_SETTINGS_255D mode=02 no-settings-tilemap")
  end
end

local function onRosterChoose()
  rosterChooseCount = rosterChooseCount + 1
  print(string.format("HORSE_ROSTER_CHOOSE count=%d mode=%02X players=%02X",
    rosterChooseCount, read(0xFF8F), read(0xFF91)))
end

local function onModeEntry()
  horseReached = true
  horseFrames = 0
  horseState("ENTRY_0CDF")
end

local function onTurnStart()
  turnCount = turnCount + 1
  if firstTurnFrame == nil then firstTurnFrame = horseFrames end
  horseState("TURN_0D57")
  if firstTurnFrame == horseFrames then
    print("HORSE_TILE_24=" .. bytes(0x8240, 16))
    print("HORSE_TILE_76=" .. bytes(0x8760, 16))
    horseBgRows("INITIAL", 0, 6)
    horseHudSeen = read(0x9823) == 0 and read(0x9830) == 0 and
      bytes(0x98A0, 20) == "C0C0CBD3D8D1CFC0C02627C0CCCBDCD5D6CFE3C0"
  end
end

local function onCallerControl()
  callerSeen = true
  horseState("CALLER_7AEA")
end

local function onMatchControl()
  matchSeen = true
  horseState("MATCH_7AFD")
end

local function onSaveSpot()
  saveSeen = true
  horseState("SAVE_SPOT_0E36")
end

local function onLetter()
  letterSeen = true
  horseState("LETTER_0E26")
end

local function onLetterDraw()
  letterDrawSeen = true
  horseState("DRAW_HORSE_7BA8")
end

local function onLaunch()
  horseState("LAUNCH_7C58")
end

local function onMake()
  makeSeen = true
  makeFrame = horseFrames
  horseState("MAKE_1E0E")
end

local function netCells()
  return string.format("%02X%02X/%02X%02X/%02X%02X",
    read(0x9849), read(0x984A), read(0x9869), read(0x986A),
    read(0x9889), read(0x988A))
end

local function onSoundSelected()
  capturedCommand = read(0xC193)
  if horseReached then
    print(string.format(
      "HORSE_SOUND frame=%d command=%02X program=%02X priority=%02X",
      horseFrames, capturedCommand, read(0xDD72), read(0xC194)))
  end
  if capturedCommand == 0x08 and makeFrame ~= nil and
      horseFrames == makeFrame + 20 then net08TimingSeen = true end
  if capturedCommand == 0x05 and makeFrame ~= nil and
      horseFrames == makeFrame + 65 then score05TimingSeen = true end
  if capturedCommand == 0x0A and matchSeen then missContact0ASeen = true end
  if capturedCommand == 0x07 then
    command7Seen = true
    captureUntil = horseFrames + 50
  end
end

local function onApuWrite(address, value)
  if capturedCommand == 0x07 and horseFrames <= captureUntil and
      address >= 0xFF10 and address <= 0xFF14 then
    apu7Seen = true
    print(string.format(
      "HORSE_APU frame=%d command=07 address=%04X value=%02X",
      horseFrames, address, value))
  end
end

local function onShotResolved()
  horseState("RESOLVED_0D8F")
end

local function onInputPolled()
  totalFrames = totalFrames + 1
  local input = {
    a = false, b = false, start = false, select = false,
    up = false, down = false, left = false, right = false
  }
  if not horseReached then
    if not menuReached and totalFrames % 30 == 1 then
      input.start = true
    elseif menuReached and totalFrames % 30 == 1 then
      if rosterChooseCount >= 2 and not opponentMoved then
        input.right = true
        opponentMoved = true
      elseif menuMoves < 2 then
        input.down = true
        menuMoves = menuMoves + 1
      else
        input.start = true
      end
    end
  else
    horseFrames = horseFrames + 1
    -- Let the first caller move to a visible spot, gather, and release.
    if firstTurnFrame ~= nil then
      local turnDelta = horseFrames - firstTurnFrame
      if turnDelta >= 10 and turnDelta <= 55 then input.right = true end
      input.a = turnDelta == 30 or turnDelta == 72
    end
  end
  emu.setInput(input, 0)
end

local function onEndFrame()
  if horseReached then
    local cells = netCells()
    if not string.find(cells, "FF") and cells ~= lastNetCells then
      print(string.format("HORSE_NET_1ECC frame=%d cells=%s",
        horseFrames, cells))
      lastNetCells = cells
    end
    if makeFrame ~= nil then
      local delta = horseFrames - makeFrame
      if delta == 20 and cells == "6162/6768/696A" then
        netBendSeen = true
      elseif delta == 35 and cells == "6B6C/6D6E/6F70" then
        netDeepSeen = true
      elseif delta == 50 and cells == "6162/6768/696A" then
        netReturnSeen = true
      elseif delta == 65 and cells == "6162/6364/6566" then
        netRestSeen = true
      end
    end
  end
  if horseReached and letterSeen and command7Seen and turnCount >= 3 and
      horseFrames >= 1150 and not stopping then
    stopping = true
    horseState("SUMMARY")
    expect(read(0xFF8F) == 0x02, "mode dispatcher did not retain ID $02")
    expect(read(0xFF91) == 0x01, "one-human player-count byte changed")
    expect(settingsDispatcherSeen and settingsBypassSeen,
      "$22EF mode-$02 settings bypass was not observed")
    expect(rosterChooseCount >= 2,
      "mode 2 did not execute both $40F4 roster accepts")
    expect(callerSeen, "$7AEA caller controller was not reached")
    expect(saveSeen, "$0E36 did not save the called spot")
    expect(makeSeen, "first caller shot did not reach $1E0E")
    expect(matchSeen, "$7AFD matcher placement was not reached")
    expect(net08TimingSeen, "made shot did not select command $08 at +20")
    expect(score05TimingSeen, "made shot did not select command $05 at +65")
    expect(netBendSeen and netDeepSeen and netReturnSeen and netRestSeen,
      "$1ECC net animation did not produce bend/deep/return/rest phases")
    expect(horseHudSeen,
      "$22C3/$0749 HUD did not clear colons and draw ROM-font names")
    expect(missContact0ASeen, "matcher miss did not reach command $0A contact")
    expect(letterDrawSeen, "$0E26 did not call $7BA8")
    expect(read(0xFFC4) == 0x04,
      "P2 matcher miss did not decrement +$0E from 5 to 4")
    expect(command7Seen, "$0E26 did not select audio command $07")
    expect(apu7Seen, "command $07 did not write DMG APU registers")
    expect(bytes(0x8760, 16) ==
      "00000000C3C366663C3C3C3C6666C3C3",
      "Horse X tile $76 differs from live VRAM")
    print(failures == 0 and
      "TRACE PASSED: $22C3/$0749/$7BA8/$06C0/$1ECC H-O-R-S-E HUD/net + lifecycle + $07 APU" or
      string.format("TRACE FAILED: %d mismatch(es)", failures))
    emu.stop(failures == 0 and 0 or 3)
  elseif totalFrames >= 6000 and not stopping then
    stopping = true
    print("TRACE ERROR: H-O-R-S-E mode was not reached")
    emu.stop(2)
  end
end

emu.addMemoryCallback(onMenuLoop, emu.callbackType.exec,
  0x03B9, 0x03B9, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSettingsDispatcher, emu.callbackType.exec,
  0x22EF, 0x22EF, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSettingsBypass, emu.callbackType.exec,
  0x255D, 0x255D, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onRosterChoose, emu.callbackType.exec,
  0x40F4, 0x40F4, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onModeEntry, emu.callbackType.exec,
  0x0CDF, 0x0CDF, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onTurnStart, emu.callbackType.exec,
  0x0D57, 0x0D57, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onCallerControl, emu.callbackType.exec,
  0x7AEA, 0x7AEA, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMatchControl, emu.callbackType.exec,
  0x7AFD, 0x7AFD, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSaveSpot, emu.callbackType.exec,
  0x0E36, 0x0E36, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLetter, emu.callbackType.exec,
  0x0E26, 0x0E26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLetterDraw, emu.callbackType.exec,
  0x7BA8, 0x7BA8, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onLaunch, emu.callbackType.exec,
  0x7C58, 0x7C58, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onMake, emu.callbackType.exec,
  0x1E0E, 0x1E0E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onShotResolved, emu.callbackType.exec,
  0x0D8F, 0x0D8F, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onSoundSelected, emu.callbackType.exec,
  0x2F9E, 0x2F9E, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addMemoryCallback(onApuWrite, emu.callbackType.write,
  0xFF10, 0xFF26, emu.cpuType.gameboy, emu.memType.gameboyMemory)
emu.addEventCallback(onInputPolled, emu.eventType.inputPolled)
emu.addEventCallback(onEndFrame, emu.eventType.endFrame)
