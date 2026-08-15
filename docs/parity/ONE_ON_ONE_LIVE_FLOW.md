# One-on-One live movement, CPU, and score-ball evidence

Last reviewed: **2026-08-15**

## Result

The focused live-flow metric is **18/18 (100.00%)**, up from the audited
pre-change state of **6/18 (33.33%)**.  This denominator is limited to the
five user-visible gaps addressed here: movement during shot gather, complete
run/shot record playback, the exact dribble-floor cue, CPU offense pacing, and
the made-ball motion before the fade.

## Recovered path and C translation

| Behavior | Ghidra / live cartridge path | Native C translation |
|---|---|---|
| Move before releasing | `$7015->$702D->$714D->$782E->$6A8C->$6B72`; `$714D` changes action to `$0A/$12` but does not clear player `+$07`. | The scene preserves held direction during gather, ticks `$6A8C` while shooting, and calls `allstar_one_on_one_rom_player_move_6b72` at every normal record boundary. |
| Full run and shot frames | `$6A8C` indexes `$6C60`, reloads the record timer, writes display frame `+$01`, moves, then increments `+$03`. | The extracted 24-action record map remains authoritative; the live scene no longer suspends it during shot actions. |
| Dribble-floor sound | `$6F2A->$6FE5->$2F88`: record `+$03=6` selects command `$0C`; `$2FB0` maps it to program `$02`, priority `$13`. `$3014` retriggers channel 2 for all six record frames. | Asset-pack v9 stores stream `$3D7F`, instrument state `NR21=$7A/NR22=$F1`, zero frequency `$000`, and six frames. `ALLSTAR_SFX_DRIBBLE` is bound to this decoded program and retains same-voice restart semantics. |
| Shoe cue cadence | `$782E->$78DD` compares the selected action with the current action and returns equal before `$78E0->$2F88`. | C now reports an action change only when the byte differs, eliminating the former command-`$0D` restart every record boundary. |
| Real-time host cadence | Cartridge logic runs at 59.7275 Hz; normal animation records observed by Mesen last six frames. | Win32 now keeps an absolute 59.7275 Hz deadline with 1 ms timer resolution, so `Sleep(1)` overshoot no longer accumulates as visible slowdown. |
| CPU does not instantly shoot | `$7170->$72BF->$72EA->$74BB` drives to a side target; arrival advances through `$732C/$738D/$763B` to a roster-specific second target. `$755D` then presses A once to gather. | `AllStarAIController` has explicit first-target, route-target, and gather stages. `allstar_ai_rom_route_target_732c` contains the exact 27 roster keys, route families, thresholds, and coordinate tables. |
| CPU release timing | After `$755D`, `$756C` reads current player animation record `+$03`, stored `$C141`, live `$FFFB`, profile table `$7638`, and skill table `$7632`. It raises A only when the record qualifies. | The AI reads `p2_animation.record_index`, stores the gather random byte, and exposes a release edge only when `allstar_ai_rom_should_shoot_756c` succeeds. It no longer manufactures record `4..7` from distance and launches immediately. |
| Made ball before fade | `$1E0E` pins X/Y and sets VZ `$FFE8`; 35 gravity-delay frames are followed by `$7BE8`. Ground crossings use `$1E5B/$1E77`: hard bounce at `+76`, smaller bounces at `+121/+158`, then `$27C7` starts at `+180`. | The score presentation sets the exact 35-frame delay and hard-bounce latch, then calls the shared ROM 8.8 integrator for every underlying state. The intentional native 3x presentation rate is unchanged. |

## Live proof

`trace_one_on_one_live_flow.lua` held Right through one complete action `$10`
cycle, pressed A without releasing Right, and observed:

- nine record-pointer values and all four run display frames;
- shot action `$0A` retaining input byte `$01`;
- eight gather record values;
- player X moving from `$7A` to `$88` before `$7C58` release.

`trace_one_on_one_cpu_decision.lua` fixes the cartridge entropy inputs, observes
the controller choose `$40/$98`, arrive, choose the roster-family `$24/$64`
route target, drive there, press gather A, run 25 `$756C` checks, and release
on shot record `$05` exactly 25 frames after gather—not on possession entry.

`trace_one_on_one_presentation_audio.lua` captured six consecutive command
`$0C` retriggers. Each writes `FF16=7A`, `FF17=F1`, `FF18=00`, `FF19=80`;
Mesen reports program `$82` (active bit plus ID `$02`) and priority `$13`.

`trace_one_on_one_score_presentation.lua` records the exact score-ball state:
`Z=$3820,VZ=$FFE8` at contact, first bounce `Z=$0000,VZ=$0153` at `+76`,
second bounce `Z=$0000,VZ=$0117` at `+121`, and fade entry at `+180`.

## Reproducible verification

```powershell
.\build\allstar_port.exe --test-all
python tools\check_one_on_one_live_flow_coverage.py

.\build\allstar_port.exe --build-assetpack "<NBA All-Star Challenge.gb>" build\one_on_one_v9.pack
.\build\allstar_port.exe --export-rom-sfx build\one_on_one_v9.pack `
  build\live_flow_proof\command_05_score.wav `
  build\live_flow_proof\command_0D_squeak.wav `
  build\live_flow_proof\command_0C_dribble.wav `
  build\live_flow_proof\command_0F_roster_navigation.wav `
  build\live_flow_proof\command_0E_player_select_match_start.wav `
  build\live_flow_proof\command_09_rim.wav
```

The six WAVs are decoded from the same reviewed ROM source region and carry
FNV-1a checksum `A0245071` in the serialized pack.
