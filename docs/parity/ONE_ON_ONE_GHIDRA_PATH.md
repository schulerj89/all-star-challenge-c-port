# One-on-One Ghidra path converted to C

## Result

The remaining 50-item One-on-One focus is verified at **50/50 (100%)**.
This means the fixed RNG, animation/assets, and collision/contact-rule
requirements in `ONE_ON_ONE_REMAINING_COVERAGE.json` are covered. It does not
mean every function in the cartridge or every game mode is complete.

Two earlier assumptions were corrected by following the live call graph:

- ordinary player contact has no hit-stun, recoil animation, knockback, or
  player velocity object;
- a rebound/pickup has no dedicated pickup action assignment.

The C port preserves those absences instead of adding behavior not present in
the cartridge.

## Shooting through the next inbound

| ROM path followed in Ghidra | Recovered behavior | Native C path |
|---|---|---|
| `$702D->$714D->$6A8C->$6B72->$6C4D->$7F37` | Gather/release follows the live animation record; direction `+$07` remains active so normal records move the shooter before release, while signed lift raises shooter and ball. | The live scene preserves gather direction and ticks the shared record/movement engine; `allstar_one_on_one_rom_shot_jump_height_6c4d` drives lift. |
| `$7170->$72EA->$74BB->$732C->$755D->$756C` | CPU drives through a side target and roster-specific route target, enters gather, then releases only when current shot record `+$03` passes the profile/skill gate. | `allstar_ai_rom_offense_target_72ea`, `allstar_ai_rom_route_target_732c`, explicit offense stages, and record-gated `rom_shot_release`. |
| `$7C58->$7EA9->$7BE8->$1CED->$1E0E` | Select the distance/profile/record arc, integrate 8.8 motion, then accept only an exact score cell. | The ROM launcher and fixed physics contact dispatcher feed `allstar_one_on_one_score_presentation_begin_1e0e`. |
| `$1E0E->$1F33->$1ECC` | Seed a four-step net effect, then write bend/deep/bend/rest tile sets at `+20/+35/+50/+65`. | The score helper exposes the exact frame and the renderer replaces the same six BG cells from the extracted 17-tile stream. |
| `$1F26->$2F88` | Select net-impact sound command `$08` with the first bend at `+20`. | The score presentation emits `NET_SOUND`; the scene plays `ALLSTAR_SFX_SWISH`. |
| `$1E0E->$1F23/$1F06->$2F88` | At `+65`, commit the score and select command `$05`, whose `$2FB0` word is `$640C`. | The scene updates the score and plays asset program `$0C`, decoded from `$3EF6/$3F00`. |
| `$2F88->$3014->$32A9->$347B->$35F0/$3631` | Commands `$04/$05/$09/$0C/$0D/$0E/$0F` select programs `$0A/$0C/$0B/$02/$11/$12/$07`; the driver loads instruments, notes, durations, pitch modulation, and square/noise APU registers. | Asset-pack v10 decodes all seven focused programs, including the dual-square foul cue, rim noise, dribble retriggers, and accepted-player chime. |
| `$2DD2->$21FA->$2933/$293D->$2945->$2A2B` | Selected roster records are copied to `$C23B/$C254`; byte zero selects exact per-slot OBJ palettes. Player frames then come from three shared action-family tile stores—there is no per-roster gameplay body-sheet table. | The renderer maps `$90/$91` to P1 `$E4/$D9` and P2 `$E0/$D0` while composing the shared extracted animation frames. |
| `$711F/$714D->$7138` | Add eight to raw player X and compare with `$54`; set `+$02` bit 4 on the left, clear it on the right, forcing the shot toward the hoop. | Both human and CPU gathers call `allstar_one_on_one_rom_shot_horizontal_flip_7138` before loading `$0A/$12`. |
| `$2B14->$0A78->$077D->$2B88` | A steal uses the live `$C0A3/$C0A7` ball point, both stored `+$10` directions, and shared `$C12D` cooldown. Success changes owner/`$FFD1` in place; it never calls the score fade. | The scene uses `$6F2A` presentation coordinates, previous direction bytes, recovery cooldown, and preserves positions/animation records across transfer. |
| `$2C50->$2CCA->$0AC5->$05A3->$0C49->$20F7` | A 25-update contact latch qualifies CHARGING/BLOCKING, draws `$065A/$066B`, sends command `$04`, waits/fades, and restarts opposite the offender. Live offsets are BGP/LCDC `+136/+147/+158`, restart `+160`, reverse `+177/+188/+199`, resume `+203`. | The native foul state presents the message, decoded program `$0A`, exact palettes/visibility, and possession restart without treating a block or charge as a score. |
| `$1E0E->$7BE8->$1E5B/$1E77->$27C7` | Made ball holds gravity 35 frames, bounces at `+76/+121/+158`, and begins fade at `+180`. | Score presentation advances the shared 8.8 integrator with the exact gravity delay and hard-first-bounce state. |
| `$0B80/$0B9A->$0C13->$2D08` | Suspend normal match input through the post-score counted holds. | `allstar_one_on_one_score_presentation_tick_0c13` advances at the fixed 60 Hz physics step. |
| `$27C7->$27EA` | Fade BGP through `E4,F9,FE,FF` in 34 frames. | The presentation exposes each byte and `allstar_renderer_apply_dmg_bgp` applies it after scene drawing. |
| `$20F7->$2197->$21C8/$21E1->$27CC->$27EA->$2D08->$702D` | Rebuild possession at `+214`: the new owner takes out at `$4C/$98`, the prior scorer uses `$4C/$88`, and the ball seeds at `$50/$90`; then reverse the palette and restore playable input at `+258`. | `allstar_one_on_one_rom_inbound_placement_20f7` maps these to native centers `(84,152)` and `(84,136)` before the same fade/input timeline. |

The cartridge script `trace_one_on_one_score_presentation.lua` proves every
boundary above. The focused shooting denominator was therefore expanded from
the former contact-only 12 items to 22 items; it moved from **12/22 (54.55%)**
to **22/22 (100.00%)** once this complete path was converted.

The pure C state machine retains those ROM-frame boundaries for regression
proof. The live native scene deliberately consumes three presentation frames
per display frame, shortening score contact through playable inbound from
about 4.3 seconds to 1.43 seconds while preserving the recovered event order.

## Per-frame contact and movement path

| ROM path followed in Ghidra | Recovered behavior | Native C path |
|---|---|---|
| `$100F -> $2C50 -> $2CCA` | Consume the previous movement update's per-player `+$0C` contact latch before movement runs again. | `allstar_one_on_one_rom_contact_tick_2c50` in `src/gameplay/allstar_one_on_one.c`, called first in each 60 Hz animation/update step. |
| `$2CCA -> $0A78` | Clear contact for no possession or protected actions; otherwise reload/decrement the unsigned `+$0D` counter from `$19`. | `allstar_one_on_one_rom_contact_counter_2cca`. |
| `$2CCA -> $0AC5` | At counter expiry, test the possession owner's `+$16` orientation and one exact unsigned player spacing. | `allstar_one_on_one_rom_contact_alignment_0ac5`. |
| `$0AC5 -> $05A3 -> $20F7` | Owner offender is charging; defender offender is blocking. Restart possession for the player opposite the offender. | `AllStarRomContactEvent` plus live `one_on_one_reset_possession` dispatch. |
| `$100F -> $6FF3 -> $7015 -> $702D` | Update player input/action state after the rule dispatcher. | Human input capture and CPU intent in `scene_one_on_one.c`. |
| `$702D -> $782E -> $6A8C` | At an animation-record boundary, choose the movement/idle action and load its next duration/frame record. | `allstar_one_on_one_rom_select_movement_action_782e` and `allstar_one_on_one_rom_animation_tick_6a8c`. |
| `$6A8C:$6B5F -> $6B72` | Clear the per-player contact latch, then dispatch requested axes in right, left, up, down order. | `allstar_one_on_one_rom_player_move_6b72`. |
| `$6B72 -> $6BAD/$6BBA/$6BD4/$6BC7` | Apply court limits and attempt a direct four-pixel coordinate update. | The same C movement helper, called only for a newly loaded normal record. |
| movement callback `-> $6E3C -> $6EC0/$6EEA` | Probe four pixels, classify asymmetric X/Y contact sides, and return before displacement when blocked. | `allstar_one_on_one_rom_player_pair_blocks_6e3c`, `allstar_one_on_one_rom_player_x_side_6ec0`, and `allstar_one_on_one_rom_player_y_side_6eea`. |
| `$6E3C -> $6F14/$6F18` | Store clear/blocked result; a block sets player `+$0C` and global `$C16B`. | Per-player `AllStarRomPlayerContactState.blocked_contact`; the live movement callback applies zero displacement on block. |
| `$7170 -> $75CD` | Consume `$C16B` for CPU rerouting: owner contact reroutes and forces A on count 14; defender contact saves its position and holds it for ten counts. | `allstar_ai_rom_contact_response_75cd` in `src/gameplay/allstar_ai.c`. |

`trace_one_on_one_player_collision.lua` observes the complete movement chain:
blocked overlap produces `$C16B=1`, X remains unchanged, and action remains
unchanged; moving away or separating vertically produces `$C16B=0` and the
exact four-pixel update. `trace_one_on_one_contact_rules.lua` separately
forces and verifies charging, blocking, and protected shot action `$0A`.

## Loose-ball recovery path

| ROM path | Recovered behavior | Native C path |
|---|---|---|
| `$2AE2 -> $2B07 -> $0A78 -> $077D` | Tick the cooldown, reject protected actors, and test the exact loose-ball rectangle. | `allstar_one_on_one_rom_recovery_dispatch` and the existing `$077D` contact helper. |
| `$2B07 -> $2B88` | Apply score/transition/first-flight/cooldown gates, set `$C12D=20`, clear flight inputs, set `$FFCF`, and report whether owner changed. | Recovery dispatch plus `allstar_one_on_one_match_take_possession`. |
| return to `$7015 -> $782E -> $6A8C` | Continue normal player animation selection. No pickup action is assigned and animation fields `+$00..+$04` are untouched. | Recovery no longer forces `$13/$0D`; the current animation record is preserved. |
| `$2B88 -> $FFD1`, then `$6F2A -> $78E9 -> $794B/$796C` | A defensive recovery sets the changed-owner flag. It remains set while the held ball is inside the central region and clears outside it; `$7C58` refuses a shot while set. | `take_back_required` is set only on a live owner change, uses the exact held-ball coordinates/region helper, and gates both human and CPU launch. |
| `$1CED -> $1D8C/$1F5F -> $1F4D` | A rim miss receives its exact impulse and eight-frame cooldown; an outer boundary zeroes both planar velocity words but leaves a live recoverable ball. | `allstar_physics_apply_rom_court_contacts` feeds the normal recovery/take-back route instead of creating an unverified sideline inbound. |

## Rendering and user-ROM asset path

| ROM path | Native conversion |
|---|---|
| `$024A -> $1FFA/$20BA` | Asset-pack builder expands 42 ball/shadow source tiles from bank 1 `$62A6..$640E`. |
| `$1FFA->$2021` and `$2219` | Builder decodes the separate 17-tile net/HUD stream from bank 3 `$793F..$7A22`. |
| `$0B9A -> $04B1(A=1) -> $050F` | Builder decodes 86 court tiles from bank 3 `$7A23..$7E47` and the 640-byte, 32-stride map from `$7E48..$7F68`. `$2243` is another mode and is not credited. |
| `$2933/$293D -> $2945 -> $2A2B` | Builder extracts 563 player tiles and 60 frame maps; `allstar_renderer_rom_player_tiles_2945` performs normal/flipped 3-by-3 8x16 traversal. |
| `$7F37 -> $6F2A/$6FEA` | `$7F37` supplies shot/gather placement; final held-ball presentation reads exact player `+$05/+$06` (visual Y is ground minus 40) and uses action-, facing-, and record-indexed `$6F2A` placement. |
| `$6945 -> $69F5 -> $6A4C/$6A5C` | `allstar_renderer_rom_ball_presentation_6945` selects eight X phases, rear-side rotation, exact `Y-Z`, and all three shadow tiers. |

The extracted bytes exist only in a user-built version-10 asset pack. The old
tracked `allstar_court_art.h` derived-art array was removed, and the renderer
uses a source-free procedural fallback when no pack is supplied.

## Verification commands

```powershell
.\build.ps1
.\build\allstar_port.exe --test-all
.\build\allstar_port.exe --build-assetpack "<user ROM>" build\allstar.assetpack
.\build\allstar_port.exe --dump-screenshots build\one_on_one_screenshots build\allstar.assetpack
.\build\allstar_port.exe --export-rom-sfx build\allstar.assetpack build\command_05.wav build\command_0D.wav build\command_0C.wav build\command_0F.wav build\command_0E.wav build\command_09.wav build\command_04.wav
.\tools\ghidra\run_ghidra_decomp.ps1
```

Headless Mesen evidence is in:

- `tools/emulator/trace_one_on_one_player_collision.lua`
- `tools/emulator/trace_one_on_one_contact_rules.lua`
- `tools/emulator/trace_one_on_one_assets.lua`
- `tools/emulator/trace_one_on_one_shot_results.lua`
- `tools/emulator/trace_one_on_one_score_presentation.lua`
- `tools/emulator/trace_one_on_one_presentation_audio.lua`
- `tools/emulator/trace_one_on_one_miss_take_back.lua`
