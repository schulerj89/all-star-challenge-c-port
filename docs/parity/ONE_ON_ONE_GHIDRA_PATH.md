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

## Rendering and user-ROM asset path

| ROM path | Native conversion |
|---|---|
| `$024A -> $1FFA/$20BA` | Asset-pack builder expands 42 ball/shadow source tiles from bank 1 `$62A6..$640E`. |
| `$024A -> $2243 -> $04B1/$050F` | Builder decodes 86 court tiles from bank 3 `$7A23..$7E47` and the 640-byte, 32-stride map from `$7E48..$7F68`. |
| `$2933/$293D -> $2945 -> $2A2B` | Builder extracts 563 player tiles and 60 frame maps; `allstar_renderer_rom_player_tiles_2945` performs normal/flipped 3-by-3 8x16 traversal. |
| `$6945 -> $69F5 -> $6A4C/$6A5C` | `allstar_renderer_rom_ball_presentation_6945` selects eight X phases, rear-side rotation, exact `Y-Z`, and all three shadow tiers. |

The extracted bytes exist only in a user-built version-4 asset pack. The old
tracked `allstar_court_art.h` derived-art array was removed, and the renderer
uses a source-free procedural fallback when no pack is supplied.

## Verification commands

```powershell
.\build.ps1
.\build\allstar_port.exe --test-all
.\build\allstar_port.exe --build-assetpack "<user ROM>" build\one_on_one_v4.pack
.\build\allstar_port.exe --dump-screenshots build\one_on_one_screenshots build\one_on_one_v4.pack
.\tools\ghidra\run_ghidra_decomp.ps1
```

Headless Mesen evidence is in:

- `tools/emulator/trace_one_on_one_player_collision.lua`
- `tools/emulator/trace_one_on_one_contact_rules.lua`
- `tools/emulator/trace_one_on_one_assets.lua`
