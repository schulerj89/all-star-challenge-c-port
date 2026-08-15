# One-on-One Shooting Evidence

Last reviewed: **2026-08-15**

## Implemented scope

The native One-on-One scene now uses a staged human shot instead of launching the ball on the first A press:

| Input or event | Native result |
|---|---|
| First A press while possessing the ball | Begin the jump/gather and retain the ball. |
| Second A press during the gather | `$702D` sees new-input bit 0 and releases immediately from phase 0, using the held-action `$7F37` origin. |
| B held during the gather | `$702D` sets phase 1 and the one-frame `$C16A` latch; the next update advances to phase 2 and releases from the phase/variant `$7F37` origin. |
| Gather reaches the 67-frame action terminal without release | Call traveling, award the defender possession, and reset the shot clock. |
| CPU enters its shooting state | Use the same shot-launch path as the human player. |
| Shot timing advances | `$6C90` starts player `+$03=0`; `$6A8C` loads the 12 three-byte records and advances `+$03`. The native gather derives the same loaded-record pointer instead of using the former fixed index 2. |
| Shot launches | `$07B4/$7EC4` selects distance class 0..4; class 0 uses the 32-frame `<<3` vector and classes 1..4 use the 64-frame `<<2` vector. `$2F40`, distance, and the live `+$03` record select the exact `$7C58` vertical byte. |
| Release height | `$6A8C` applies the signed `$6C4D[+$03]` lift to player visual Y; `$7F37` composes the ball from player `+$05/+$15`. A normal release consequently rises through `$26,$2F,$36,$3B,$3E,$40,$40,$3E,$39,$32,$2A,$26`, rather than always launching at 48 pixels. |
| Ball enters the hoop region | `$1CED` uses exact integer-byte windows around `x=$54`, `y=$5D/$5E/$5F`, and `z=$37..$3D`; One-on-One no longer uses the provisional descending plane or five-pixel radius. |
| Rim/backboard miss | Apply the recovered raw X impulses, VZ reflection/loss, and eight-frame `$C17E` contact cooldown. |
| Ground contact | `$1E77` clears the recovery flight lock, negates raw VZ, and subtracts `$0039` (or one `$012C` hard-bounce loss when pending). |
| Shot action animation | Use the recovered 67 frame-duration total from the action `$0A/$12` animation tables instead of the former arbitrary 15-frame timer. |
| Miss travels behind the hoop (`y<$5C`) | Apply `$1CED`: return it to `y=$5E` with the recovered small positive court velocity. |
| Ball reaches `x<$0A`, `x>=$A0`, or `y>=$97` | Apply `$1CED->$1F4D`: zero planar velocity and leave possession unresolved until recovery. |
| Loose ball recovery | Use `$077D`'s strict `|dx|<12`, `|dy|<8` collision limits, then apply possession through the existing match state. |
| Defender jumps into a live shot | `$2B6C` can reach the shared transfer gate, but `$2B88` rejects it while `$FFF8=1`; the ROM has no separate goaltending call or live-block award. After first contact clears `$FFF8`, the jump-height catch band can recover the rebound. |

The pure shot state machine and possession penalty are covered by:

```powershell
.\build\allstar_port.exe --test-one-on-one-shooting
```

`--test-all` includes this suite.

## ROM and Ghidra basis

The recovered bank-1 shooting cluster provides the structural basis for this change:

| ROM routine | Observed behavior | Native counterpart |
|---|---|---|
| `$702D` | In action `$0A`, new-A at player `+$11` bit 0 launches directly while phase is zero. Held-B at `+$12` bit 1 sets phase 1 and `$C16A=1`; the following update clears the latch, advances to phase 2, updates `$7F37`, and launches. | `allstar_one_on_one_shot_input` and `allstar_one_on_one_shot_tick` preserve the distinct immediate and latched paths. The scene passes the resulting ROM phase into the release-origin helper. |
| `$7BE8` | Once per frame, subtracts `$000F` from 8.8 vertical velocity, applies raw `+/-2` planar friction while the height integer byte is zero, then integrates the three velocity/position pairs. | `allstar_physics_rom_step_7be8` reproduces the exact 16-bit operations and `allstar_physics_update_ball` uses that state as its canonical fixed step. |
| `$7C58` | Uses `$07B4` rectangles and `$7EC4` corner cells for distance class 0..4, player profile `+$18` from `$2F40`, and live animation record `+$03` to index the five-by-twelve low-byte table at `$7DD1`; class 0 has high byte 0 and classes 1..4 high byte 1. A nonzero player shot phase takes `$7F0A`, zeroes planar velocity, and launches at raw VZ `-$0100`. | `allstar_one_on_one_rom_shot_distance_class`, `allstar_one_on_one_rom_shot_profile`, `allstar_one_on_one_rom_shot_record_index`, `allstar_one_on_one_rom_shot_vertical_velocity`, and `allstar_physics_shoot_ball_rom_7c58` reproduce those selectors and raw launch values. |
| `$7EA9` | Shifts signed target displacement left three bits for class 0 and left two bits for classes 1..4, reaching the target in 32 or 64 8.8 steps. | The ROM launcher constructs the exact raw vector. A deterministic class-3 test reaches `$54/$5C` on step 64 and matches the traced first-step VZ `$0189`; the phase path matches `$FEF1`. |
| `$791D/$794B` | Classifies player field `+$16` as 0, 1, or 2 from the exact left/right court wedge tables at `$79B6/$79D2`. | `allstar_one_on_one_rom_shot_variant` applies the recovered thresholds after converting native center X back to ROM field `+$06`. |
| `$7F37` | Chooses ball offsets by action/facing while held and by shot phase/variant during the jump; computes height from player fields `+$05/+$15`; display frames `$00/$0C` clear the separate ball because their player frame contains it. | `allstar_one_on_one_rom_release_offset`, `allstar_one_on_one_rom_release_height`, and `allstar_one_on_one_rom_shot_release_height` reproduce launch coordinates and record-coupled height; `allstar_renderer_rom_held_ball_7f37` supplies live held-ball presentation. |
| `$6A8C` animation dispatcher | Action `$0A/$12` advances through twelve duration/frame records totaling 67 frames. At a boundary it loads the duration into `+$04`, applies `$6C4D[+$03]` to visual Y, then increments `+$03` at `$6C47`. Because `$7015` calls `$702D` first, release observes that loaded-record pointer. | `allstar_one_on_one_rom_shot_record_index` reproduces pointer timing; `allstar_one_on_one_rom_shot_animation_frame` reproduces display records and phase overrides. |
| Fixed `$1CED` | Dispatches outer limits, back-court return, exact score cells, front/side/back rim and backboard responses, `$C17E` cooldown, and ground/bounce behavior. It does not perform a native descending-plane or circular-radius test. | `allstar_physics_apply_rom_court_contacts` works on the canonical 8.8 bytes and emits explicit score/rim/backboard events; boundary tests cover the score cell, side impulse, back-rim reflection, and cooldown. |
| Fixed `$1E5B/$1E77` | Treats exact zero or wrapped negative height (integer `>=$E0`) as ground, clears `$FFF8`, negates raw VZ, and applies the One-on-One `$0039` loss or one `$FFD4` `$012C` loss. | The fixed-step updater applies the same raw arithmetic and exposes recovery on first ground contact. |
| Fixed `$1F4D` | Zeroes the two planar 8.8 velocity words. | `allstar_physics_apply_rom_court_contacts` zeroes native `vx` and `vy`; deterministic boundary tests cover the semantic result. |
| Fixed `$077D` | Tests loose-ball proximity against player reference coordinates with strict Y `<8` and X `<12` limits. | `allstar_one_on_one_player_can_pick_up_ball` reproduces the strict limits and is boundary-tested. |
| Fixed `$2AE2/$2B07/$2B88` | Decrements the pickup cooldown, applies possession/`$FFEB`/height gates, tests player 1 then player 2 for action and collision eligibility, applies `$FFE2/$FFE7/$FFF8` final locks, and reloads a 20-frame cooldown on award. | `allstar_one_on_one_rom_recovery_dispatch` preserves that order and is boundary-tested; the scene supplies the score-event, transition, first-contact flight, and exact proximity states. |

Every successful possession award also resets the native shot-attempt wrapper
to `IDLE`. This is native bookkeeping around the recovered dispatcher: leaving
the wrapper in `RELEASED` previously caused the first recovered shot to lock
out all later A-button gathers.

The static control flow is paired with headless Mesen traces. In addition to
`tools/emulator/trace_one_on_one_input.lua`,
`tools/emulator/trace_one_on_one_shot_results.lua` proves a complete cartridge
make. At release frame 37 it observes action `$0A`, player `+$03=$07`,
`+$18=$02`, visual/ground Y `$56/$98`, and `$7F37` ball Z `$40`. `$7C58`
then writes class 3, vector `VX=$0004`, `VY=$FF18`, `VZ=$01C8`; after `$7BE8`
integration, fixed `$1CED` enters `$1E0E` at
`X/Y/Z=$54/$5C/$38` with `$FFD7=1`.

From a captured gameplay state the input trace
asserts the A-A path (`$FFAE=1`, phase 0, immediate possession transfer) and the
A-B path (`$FFAF=2`, phase 1/`$C16A=1`, then phase 2/release one update later).
The native test exercises the same state transitions. Landing with the ball at
the 67-frame action terminal remains traveling.

## Shot-result coverage

`ONE_ON_ONE_SHOT_RESULT_COVERAGE.json` fixes this area at 12 equal
requirements. The audited pre-fix state was **9/12 (75.00%)**: distance,
profile, table bytes, planar vectors, gravity, contact cells, and miss response
were present, but the live record selector, record-coupled release height, and
an end-to-end native scene make were missing. They are now **12/12 (100.00%)**.

## Known deviations

The scoped launch/contact path is verified, but complete gameplay parity is not:

- player collision penalties and unrelated `$7170` branches remain unmapped;
- `$1CED` presentation/effect sequencing and branches belonging to other game modes remain outside this One-on-One claim.

The former One-on-One implementation used an interpolated descending plane at `z=16` plus a five-pixel circle. Recovered `$1CED` proves that model was not native: the ROM compares integer 8.8 bytes in discrete score/contact cells and never reads vertical-velocity sign in the score decision. One-on-One now consumes the exact contact event; the generic plane helper remains available only for the other prototype shooting scenes.

The former fix treated native `y<76` and other reachability limits as an immediate out-of-bounds turnover. The traced ROM does not do that. `$1CED` treats `y<$5C` as a return contact, while only `x<$0A`, `x>=$A0`, or `y>=$97` call `$1F4D`; that helper stops planar motion without awarding possession. Possession is resolved later by `$2AE2/$2B07/$2B88` after `$077D` reports player contact. The native scene now follows that separation.

Native player X is a center coordinate corresponding to ROM field `+$06 + 8`; native Y corresponds to ground field `+$15`. Release X therefore subtracts that center bias after `$7F37`, while recovery compares the ball to native X and native Y minus two exactly as `$077D` does.

Player movement uses the same normalized coordinates. `$6BAD/$6BBA` constrain ROM field `+$06` to `8..148`, producing native center X `16..156`; `$6BC7/$6BD4` constrain field `+$15` to `98..152`. The scene routes movement through the boundary-tested `allstar_one_on_one_rom_clamp_player_court` helper.

For the remaining deviations, whole-project `behavior.one_on_one_rules`, `behavior.physics`, `behavior.ai`, and `behavior.animation` remain partial. The focused One-on-One manifest gives credit only to the exact launch/contact requirements covered here.
