# One-on-One presentation and audio evidence

Last reviewed: **2026-08-15**

## Result

The focused event/path metric is **60/60 (100.00%)**, up from the reconstructed
pre-audit state of **6/20 (30.00%)**. This metric covers exact ROM event timing,
animation selection, roster-specific OBJ palettes, shot facing, live steals,
charging/blocking, ball placement, miss recovery and take-back, plus the focused
`$04/$05/$09/$0C/$0D/$0E/$0F` audio programs. It does not claim that the
entire cartridge music/APU interpreter is ported; these seven cues are decoded
from the user's ROM into asset-pack v15 and rendered from recovered square/noise
state.

## Ghidra and live-ROM path

| User-visible behavior | Recovered ROM path | Native C path |
|---|---|---|
| Score effect starts | `$1E0E->$1F33` writes `$C129=4`, `$C168=$14` | `allstar_one_on_one_score_presentation_begin_1e0e` initializes the exact state clock. |
| Net bends | `$1ECC` at `+20` writes `$61,$62/$67,$68/$69,$6A` | `allstar_one_on_one_score_net_frame_1ecc` selects `BEND`; renderer replaces the same six cells. |
| Net reaches deepest frame | `$1ECC` at `+35` writes `$6B,$6C/$6D,$6E/$6F,$70` | The helper selects `DEEP`. |
| Net returns and rests | `$1ECC` at `+50` repeats bend; at `+65` restores `$61,$62/$63,$64/$65,$66` | The helper returns `BEND`, then `REST`. |
| Ball/net priority | `$1E0E` seeds `$C12B=$23`; `$6945->$69F5` ORs OAM attribute bit 7 while it is nonzero, so the score ball is behind nonzero net BG pixels for 35 updates while player OAM remains in front. | Native draws the priority ball between the base court and the extracted net overlay through frame 34, then returns it to the ordinary loose-ball layer. The four ROM net tile states remain discrete. |
| Net impact sound | `$1F26->$2F88` selects command `$08` at `+20` | `ALLSTAR_ROM_SCORE_EVENT_NET_SOUND` plays `ALLSTAR_SFX_SWISH`. |
| Command mapping | `$2F88` indexes `$2FB0`; `$05 -> $640C` and `$0D -> $1411` | Asset extraction retains program IDs `$0C/$11` and priority windows `100/20`. |
| Score sound | `$1F23/$1F06->$2F88` selects `$05`; `$3014->$32A9->$347B` consumes streams `$3EF6/$3F00` with instruments `$97/$98` | Asset-pack v12 stores both decoded square channels for 72 frames; `ALLSTAR_SFX_SCORE_CHIME` renders their exact duties, envelopes, notes, retriggers, and `$3244` pitch cycle. |
| Shoe screech | `$782E->$78DD` first compares the new action with `[de]`; only a changed action reaches `$78E0->$2F88` command `$0D`. Program `$11` reads `$3FA2` and instrument `$9F`. | The selector now returns false for an unchanged action, so holding one direction does not retrigger the screech at every six-frame record boundary. |
| Post-score take-out | `$20F7->$2197` selects `$21C8` for the new owner and `$21E1` for `$FFD0`; ball seed is `$50/$90` | `allstar_one_on_one_rom_inbound_placement_20f7` places the owner at native `(84,152)` and the prior scorer at `(84,136)`. |
| Net graphics source | `$1FFA->$2021` and `$2219` decompress bank 3 `$793F` to VRAM `$9600` | Asset-pack v12 stores 17 decoded tiles for signed BG IDs `$60..$70`; native now explicitly composes the rest frame before the first score. |
| One-on-One court source | `$0B9A->$04B1(A=1)->$050F` selects bank 3 `$7A23->$9000` and `$7E48->$9800` | The builder stores 86 court tiles and the 640-byte, 32-stride map. `$2243` is not credited because it belongs to another mode. |
| Final held/dribbling ball | Per update, `$7F37` runs first and `$6F2A` then reads player `+$05/+$06`, applies action/facing offsets, and uses `$6FEA` height. The live inbound sample is `+$05=$70`, `+$15=$98`, ball `$5A/$96`. `$2945` copies player `+$02` bit 4 directly into OAM X-flip bit 5. | `allstar_renderer_rom_dribble_ball_6f2a` derives `+$05` as ground Y minus 40. The former minus-18 conversion placed the ball 22 pixels too low; the former inverted player flip made the exact ball coordinate appear beside the opposite hand. Player composition and ball-side selection now consume the same ROM bit. |
| Dribble sound | `$6F2A->$6FE5` sends command `$0C` every update while player `+$03=6`; `$2F88/$2FB0->$3014` maps program `$02`, stream `$3D7F`, priority `$13`, and channel-2 registers `$7A/$F1/$00/$80` | Asset-pack v12 binds the decoded six-frame program to `ALLSTAR_SFX_DRIBBLE` and the live scene retriggers it on that exact record cadence. |
| CPU stops dribbling and shoots | `$74BB` still requests movement at positive delta `+4`; `$6BBA` then permits the final raw-X step `$08->$04` (center `16->12`) before `$751D` advances `$72EA->$732C->$755D->$756C`. | The native clamp now admits centered X 12 and ground Y 96, matching the reachable ROM edge positions. The deterministic scene regression reaches route stage 2 and releases at frame 130 instead of being clamped to X 16 forever. |
| Defender block/rebound motion | `$70FD` selects `$05/$0C/$14`; protected `$702D` updates retain jump-edge `+$07`, `$6A8C->$6BF9->$6B72` applies it at each record boundary, and the final control record transitions to `$06/$0D/$15`. `$2B88` changes possession without rewriting action, `+$07`, `+$10`, or facing. | The scene latches block direction, moves throughout the 72-frame jump, preserves the same state across an airborne rebound, and then enters the appropriate held/free landing family without a synthetic pause. |
| Players sit on the court | `$6B5F` rebuilds ground `+$15=+$05+$28`; `$2945` writes `+$05` to OAM, whose hardware Y bias is 16. The 48-pixel stack therefore ends at ground minus 8. | The optional native floor shadow moved from `ground+1` to immediately below `ground-8`; the former nine-pixel gap was the floating illusion, not a ROM player-coordinate error. |
| Rim miss and rebound | `$1CED->$1D8C->$1F5F` installs the exact impulse and eight-frame `$C17E` cooldown, preserves initial-flight `$FFF8`, and dispatches `$2F88` command `$09`; `$1E5B/$1E77` clears `$FFF8` on the first ground bounce. | Rim/backboard contact keeps the ball in live initial flight; after the exact ground bounce makes it recoverable, CPU contest logic leaves initial-flight state for rebound behavior. The scene plays `ALLSTAR_SFX_RIM_CLANK` once per emitted rim contact. |
| Rim sound | `$2F88/$2FB0` maps `$09->$230B`; `$3014` reads program `$0B`, stream `$3EF2`, instrument `$9B`, and writes noise `NR41/42/43/44=$EB/$F2/$5A/$BF`. | Asset-pack v12 stores the 24-frame noise program. The PCM renderer uses the DMG 7-bit LFSR and `$F2` envelope instead of the former two-tone square fallback. |
| Roster navigation pitch | Bank 2 `$40F4->$4118->$2AB5/$2F88` sends command `$0F`, mapping to program `$07`, priority `$19`, stream `$3EBC`. `$347B:$34A3` adds descriptor `$8F` byte `+$01=$0A` to stream note `$47`, indexes `$51`, and writes `NR13/NR14=$B1/$BF` (`$07B1`). | Asset-pack v12 applies the descriptor transpose before `$31C6/$3159` lookup. `ALLSTAR_SFX_MENU_MOVE` now renders the live-confirmed higher pitch instead of untransposed `$0773`. |
| Accepted roster player | Bank 2 `$40F4->$410E/$2F88` sends command `$0E`, mapping to program `$12`, priority `$32`, stream `$3FA6`. | `ALLSTAR_SFX_MENU_SELECT` is bound to the extracted five-step, 48-frame chime. The final cue starts 35 frames before `$702D`, so it remains audible into match start; the synthetic match whistle was removed. |
| Selected-player gameplay appearance | `$2DD2` copies selected 25-byte records to `$C23B/$C254`; `$21FA` maps their first byte to P1 OBP0 `$E4/$D9` and P2 OBP1 `$E0/$D0`. `$2933/$293D->$2945->$2A2B` still composes three shared action-family tile stores. | The selected roster entries now drive the exact slot-specific DMG palette. Ghidra and Mesen prove there is no table of 27 distinct gameplay body sheets; portraits are separate and remain outside this pass. |
| Sideline shot direction | `$711F/$714D->$7138` compares player center X with `$54`, sets `+$02` bit 4 on the left side and clears it on the right. | Human and CPU shot gathers now force the extracted shooting frame toward the hoop rather than retaining the previous run direction. |
| Sideline shot ball attachment | `$7F37` selects `$7FC7` for action `$0A` and `$7FCB` for action `$12`; both byte rows are `07 FE 0A FE`, or `{+7,-2}/{+10,-2}` by facing. | The action-`$12` renderer no longer interprets those bytes as `{+7,+10}/{-2,+8}`, so the left-side ball stays at the shooting hand with the same height convention as action `$0A`. |
| Dunk/drop animation | `$702D` held B arms player `+$13=1` and `$C16A=1`; the following update advances to phase 2 and `$7F0A` launches with zero planar velocity/raw VZ `-$0100`. On the next record load `$6A8C:$6B34-$6B59` selects display `$13`. | The native A-then-B path retains the release update's prior frame, then forces extracted display frame `$13` on the next update while using the exact vertical drop physics. |
| CPU/live steal continuation | `$2B14->$0A78->$077D` checks the real ball point and opposing stored `+$10` directions; `$2B88` rejects active `$C12D`, otherwise changes `$FFCF/$FFD1` without score presentation. | The scene no longer manufactures opposing directions or compares to the handler center. A successful CPU steal preserves positions/animation and continues live; same-direction left movement is not an automatic steal. |
| Charging/blocking presentation | `$2C50->$2CCA->$0AC5` classifies the offender, `$05A3` draws `CHARGING`/`BLOCKING` and sends `$04`, and `$0C49->$27C7->$20F7->$27CC` waits, fades, restarts opposite offender, and resumes. | A dedicated state clock reproduces the live Mesen offsets: fade at `+136/+147/+158`, restart `+160`, reverse at `+177/+188/+199`, resume `+203`. Blocks no longer look like a frozen or scored possession. |
| Miss and outer boundary | `$1CED->$1D8C` applies the rim impulse/cooldown; X `<$0A`, X `>=$A0`, or Y `>=$97` reaches `$1F4D`, which zeros planar velocities. | The existing 8.8 contact helper is exercised by a full Mesen miss/boundary trace. After first ground contact makes it recoverable, the stopped ball remains live; the ROM does not invent a sideline inbound here. |
| Defensive recovery and take-back | `$2AE2->$2B07->$2B88` sets owner and `$FFD1=(recoverer!=$FFD0)`. `$78E9->$794B/$796C` keeps `$FFD1` inside the central region and clears it only outside. If `$7C58` sees it, it stores owner in `$FFD0/$C178`; next-update `$2C50` selects text `$067C` (“DIDN'T CLEAR BALL”), and `$05A3->$20F7` turns possession over. | Live recovery still clears correctly outside the region. An attempted inside-region shot now latches the offender, displays the ROM violation with command `$04`, follows the existing fade clock, and restarts opposite the offender. |

The Mesen traces record `$2DD2->$21FA` as P1 `$91->$D9` and P2 `$90->$E0`,
both `$7138` sideline outcomes, and the foul route as command `$04`, active
program `$8A`, priority `$1E`, restart at `+160`, and resume at `+203`.
The presentation trace also records the `$1ECC` phases at exact deltas `20/35/50/65`,
commands `$08/$05`, their selected program/priority bytes, every `$FF10..$FF25`
write for `$05/$0C/$0D/$0E/$0F`, six consecutive command-`$0C` updates at
animation record six, command `$0D` only on movement-action changes, and the
corrected roster navigation/accept mappings.
`trace_one_on_one_rim_audio.lua` separately forces the exact `$53/$5E/$37`
cell and captures command `$09`, program `$8B` (active bit plus ID `$0B`),
priority `$23`, cooldown `$08`, preserved `$FFF8`, and the four noise-register
writes. The presentation/audio trace also asserts the live `$6F2A` result from
the owner action, record, direction bit, coordinates, and `$6FEA` height table.
That same presentation/audio run reaches `$20F7` at
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
  frames, descriptor `$8F`, stream note `$47` plus transpose `$0A`, effective
  table index `$51`, frequency `$7B1`, and NR10 `$08` sweep;
- command `$0E`: accepted-player/match-start program `$12`, priority `50`,
  stream `$3FA6`, 48 frames, descriptor `$9D`, and note sequence
  `$783,$791,$79D,$783,$7AD`;
- command `$09`: rim program `$0B`, priority `35`, stream `$3EF2`, 24 frames,
  descriptor `$9B`, noise registers `$EB/$F2/$5A/$BF`;
- command `$04`: foul program `$0A`, priority `30`, streams `$3ED4/$3EE0`,
  30 frames, descriptors `$93/$94`, first frequencies `$7C1/$7BE`;
- all seven decoded assets carry FNV-1a `A0245071` over their reviewed ROM audio
  source region in the verified USA/Europe image.

The runtime never uses the old generic three-note/two-note fallbacks when a
valid version-15 user pack is loaded. The proof WAVs are reproducible from the
serialized decoded frames:

```powershell
.\build\allstar_port.exe --export-rom-sfx build\allstar.assetpack `
  build\audio_proof\command_05_score.wav `
  build\audio_proof\command_0D_screech.wav `
  build\audio_proof\command_0C_dribble.wav `
  build\audio_proof\command_0F_roster_navigation.wav `
  build\audio_proof\command_0E_player_select_match_start.wav `
  build\audio_proof\command_09_rim.wav `
  build\audio_proof\command_04_foul.wav
```

The 2026-08-15 v11 rebuild produced the original seven-cue SHA-256 proof
`8EEF558779ED3E91996EBC01ABEBAAF80F6268CDB4CFE547C93951150D80EB89`
for command `$0F` character cycling and
`3CF97D15C31128FA60E91E9590F1948E74BAE2EFD4E083A740BA77904C2ED0C3`
for command `$04` charging/blocking. Current rebuilds use asset-pack v15,
not checked-in replacement recordings.

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
- `tools/emulator/trace_one_on_one_rim_audio.lua`
- `tools/emulator/trace_one_on_one_roster_facing.lua`
- `tools/emulator/trace_one_on_one_foul_presentation.lua`
- `tools/emulator/trace_one_on_one_take_back_violation.lua`

The screenshot dump includes separated-player running and idle `$6F2A/$2945`
hand-side proof (`04d`/`04e`), shot lift, flight, score/fade/inbound, and the
extracted bend/deep/rest net frames.
The `04m` through `04q` frames prove CPU route/release, the grounded foot
baseline, defender block/landing reentry, and the exact rim-bounce state.
The new `04r` through `04z` plus `04xa` frames show roster-record palette differences,
both hoop-facing sideline gathers, an in-place CPU steal, both contact-foul
popups, the phase-two dunk, corrected left-side shot ball, and the take-back
violation popup, with `04xa` isolating the score ball behind the bent net.
