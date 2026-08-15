# One-on-One presentation and audio evidence

Last reviewed: **2026-08-15**

## Result

The focused event/path metric is **40/40 (100.00%)**, up from the reconstructed
pre-audit state of **6/20 (30.00%)**. This metric covers exact ROM event timing,
animation selection, ball placement, miss/boundary recovery and take-back, plus
the focused `$05/$0C/$0D/$0E/$0F` audio programs. It does not claim that the
entire cartridge music/APU interpreter is ported; these five cues are decoded
from the user's ROM into asset-pack v8 and rendered from recovered square state.

## Ghidra and live-ROM path

| User-visible behavior | Recovered ROM path | Native C path |
|---|---|---|
| Score effect starts | `$1E0E->$1F33` writes `$C129=4`, `$C168=$14` | `allstar_one_on_one_score_presentation_begin_1e0e` initializes the exact state clock. |
| Net bends | `$1ECC` at `+20` writes `$61,$62/$67,$68/$69,$6A` | `allstar_one_on_one_score_net_frame_1ecc` selects `BEND`; renderer replaces the same six cells. |
| Net reaches deepest frame | `$1ECC` at `+35` writes `$6B,$6C/$6D,$6E/$6F,$70` | The helper selects `DEEP`. |
| Net returns and rests | `$1ECC` at `+50` repeats bend; at `+65` restores `$61,$62/$63,$64/$65,$66` | The helper returns `BEND`, then `REST`. |
| Net impact sound | `$1F26->$2F88` selects command `$08` at `+20` | `ALLSTAR_ROM_SCORE_EVENT_NET_SOUND` plays `ALLSTAR_SFX_SWISH`. |
| Command mapping | `$2F88` indexes `$2FB0`; `$05 -> $640C` and `$0D -> $1411` | Asset extraction retains program IDs `$0C/$11` and priority windows `100/20`. |
| Score sound | `$1F23/$1F06->$2F88` selects `$05`; `$3014->$32A9->$347B` consumes streams `$3EF6/$3F00` with instruments `$97/$98` | Asset-pack v8 stores both decoded square channels for 72 frames; `ALLSTAR_SFX_SCORE_CHIME` renders their exact duties, envelopes, notes, retriggers, and `$3244` pitch cycle. |
| Shoe screech | `$782E->$78DD` first compares the new action with `[de]`; only a changed action reaches `$78E0->$2F88` command `$0D`. Program `$11` reads `$3FA2` and instrument `$9F`. | The selector now returns false for an unchanged action, so holding one direction does not retrigger the screech at every six-frame record boundary. |
| Post-score take-out | `$20F7->$2197` selects `$21C8` for the new owner and `$21E1` for `$FFD0`; ball seed is `$50/$90` | `allstar_one_on_one_rom_inbound_placement_20f7` places the owner at native `(84,152)` and the prior scorer at `(84,136)`. |
| Net graphics source | `$1FFA->$2021` and `$2219` decompress bank 3 `$793F` to VRAM `$9600` | Asset-pack v8 stores 17 decoded tiles for signed BG IDs `$60..$70`. |
| One-on-One court source | `$0B9A->$04B1(A=1)->$050F` selects bank 3 `$7A23->$9000` and `$7E48->$9800` | The builder stores 86 court tiles and the 640-byte, 32-stride map. `$2243` is not credited because it belongs to another mode. |
| Final held/dribbling ball | Per update, `$7F37` runs first and `$6F2A` then reads player `+$05/+$06`, applies action/facing offsets, and uses `$6FEA` height. The live inbound sample is `+$05=$70`, `+$15=$98`, ball `$5A/$96`. `$2945` copies player `+$02` bit 4 directly into OAM X-flip bit 5. | `allstar_renderer_rom_dribble_ball_6f2a` derives `+$05` as ground Y minus 40. The former minus-18 conversion placed the ball 22 pixels too low; the former inverted player flip made the exact ball coordinate appear beside the opposite hand. Player composition and ball-side selection now consume the same ROM bit. |
| Dribble sound | `$6F2A->$6FE5` sends command `$0C` every update while player `+$03=6`; `$2F88/$2FB0->$3014` maps program `$02`, stream `$3D7F`, priority `$13`, and channel-2 registers `$7A/$F1/$00/$80` | Asset-pack v8 binds the decoded six-frame program to `ALLSTAR_SFX_DRIBBLE` and the live scene retriggers it on that exact record cadence. |
| Roster navigation | Bank 2 `$40F4->$4118->$2AB5/$2F88` sends command `$0F`, mapping to program `$07`, priority `$19`, stream `$3EBC`. | `ALLSTAR_SFX_MENU_MOVE` is bound to the extracted 24-frame swept-square program. |
| Accepted roster player | Bank 2 `$40F4->$410E/$2F88` sends command `$0E`, mapping to program `$12`, priority `$32`, stream `$3FA6`. | `ALLSTAR_SFX_MENU_SELECT` is bound to the extracted five-step, 48-frame chime. The final cue starts 35 frames before `$702D`, so it remains audible into match start; the synthetic match whistle was removed. |
| Miss and outer boundary | `$1CED->$1D8C` applies the rim impulse/cooldown; X `<$0A`, X `>=$A0`, or Y `>=$97` reaches `$1F4D`, which zeros planar velocities. | The existing 8.8 contact helper is now exercised by a full Mesen miss/boundary trace. The ball remains live and becomes recoverable; the ROM does not invent a sideline inbound here. |
| Defensive recovery and take-back | `$2AE2->$2B07->$2B88` sets owner and `$FFD1=(recoverer!=$FFD0)`. `$78E9->$794B/$796C` keeps `$FFD1` inside the central region and clears it only outside; `$7C58` refuses launch while set. | Live recovery preserves the ball point, sets `take_back_required` on owner change, shares it with CPU offense, and blocks launch until `allstar_one_on_one_rom_take_back_cleared_78e9` succeeds. |

The Mesen trace records the `$1ECC` phases at exact deltas `20/35/50/65`,
commands `$08/$05`, their selected program/priority bytes, every `$FF10..$FF25`
write for `$05/$0C/$0D/$0E/$0F`, six consecutive command-`$0C` updates at
animation record six, command `$0D` only on movement-action changes, and the
corrected roster navigation/accept mappings.
It also asserts the live `$6F2A` result from the owner action, record, direction
bit, coordinates, and `$6FEA` height table. The same run reaches `$20F7` at
match entry and after a score and asserts both 25-byte templates, reversed
ownership, the target Y bytes `$98/$88`, and the `$50/$90` ball origin.

## Audio asset proof

The builder follows `$2F88->$3014` data instead of embedding guessed notes:

- command `$05`: program `$0C`, priority `100`, streams `$3EF6/$3F00`, 72
  audible frames, square descriptors `$97/$98`;
- command `$0D`: program `$11`, priority `20`, stream `$3FA2`, three audible
  frames, square descriptor `$9F` and NR10 sweep `$1F`;
- command `$0C`: program `$02`, priority `19`, stream `$3D7F`, six frames,
  channel-2 duty/length `$7A`, envelope `$F1`, and frequency `$000`;
- command `$0F`: navigation program `$07`, priority `25`, stream `$3EBC`, 24
  frames, descriptor `$8F`, base frequency `$773`, and NR10 `$08` sweep;
- command `$0E`: accepted-player/match-start program `$12`, priority `50`,
  stream `$3FA6`, 48 frames, descriptor `$9D`, and note sequence
  `$783,$791,$79D,$783,$7AD`;
- all five decoded assets carry FNV-1a `A0245071` over their reviewed ROM audio
  source region in the verified USA/Europe image.

The runtime never uses the old generic three-note/two-note fallbacks when a
valid version-8 user pack is loaded. The proof WAVs are reproducible from the
serialized decoded frames:

```powershell
.\build\allstar_port.exe --export-rom-sfx build\allstar.assetpack `
  build\audio_proof\command_05_score.wav `
  build\audio_proof\command_0D_screech.wav `
  build\audio_proof\command_0C_dribble.wav `
  build\audio_proof\command_0F_roster_navigation.wav `
  build\audio_proof\command_0E_player_select_match_start.wav
```

## Native pacing

The pure state helper still traverses all 258 cartridge presentation states.
The native scene now consumes them at an explicit **3x presentation rate**,
so score contact to playable inbound takes about **1.43 seconds** rather than
the cartridge's roughly 4.3 seconds. This is a documented usability override;
net and audio events still occur at their exact underlying ROM-state offsets.

Normal gameplay animation still uses the traced 59.7 Hz update and six-frame
records. The perceived extra slowdown was in the Win32 host loop: `Sleep(1)`
overshoot was made the next frame's origin, accumulating scheduler delay. The
runtime now uses a 1 ms Windows timer period and an absolute 59.7275 Hz deadline;
the renderer itself completes the 420-frame headless smoke test in about 0.05 s.

## Verification

```powershell
.\build.ps1 -RomPath "<user ROM>"
.\build\allstar_port.exe --test-all
python tools\check_one_on_one_presentation_audio_coverage.py
```

Live-ROM evidence:

- `tools/emulator/trace_one_on_one_presentation_audio.lua`
- `tools/emulator/trace_one_on_one_assets.lua`
- `tools/emulator/trace_one_on_one_miss_take_back.lua`

The screenshot dump includes separated-player running and idle `$6F2A/$2945`
hand-side proof (`04d`/`04e`), shot lift, flight, score/fade/inbound, and the
extracted bend/deep/rest net frames.
