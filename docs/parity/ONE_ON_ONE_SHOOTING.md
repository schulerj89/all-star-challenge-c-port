# One-on-One Shooting Evidence

Last reviewed: **2026-08-14**

## Implemented scope

The native One-on-One scene now uses a staged human shot instead of launching the ball on the first A press:

| Input or event | Native result |
|---|---|
| First A press while possessing the ball | Begin the jump/gather and retain the ball. |
| Second A press during the gather | Release the ball using the recovered `$7F37` phase-two origin. |
| Gather expires without release | Call traveling, award the defender possession, and reset the shot clock. |
| CPU enters its shooting state | Use the same shot-launch path as the human player. |
| Clean launch reaches frame 32 | Cross the hoop plane while descending, score exactly once, and apply post-score possession. |
| Accuracy roll misses | Aim outside the five-pixel rim radius so the miss cannot be counted as a make. |
| Shot action animation | Use the recovered 67 frame-duration total from the action `$0A/$12` animation tables instead of the former arbitrary 15-frame timer. |
| Miss travels behind the hoop (`y<$5C`) | Apply `$1CED`: return it to `y=$5E` with the recovered small positive court velocity. |
| Ball reaches `x<$0A`, `x>=$A0`, or `y>=$97` | Apply `$1CED->$1F4D`: zero planar velocity and leave possession unresolved until recovery. |
| Loose ball recovery | Use `$077D`'s strict `|dx|<12`, `|dy|<8` collision limits, then apply possession through the existing match state. |

The pure shot state machine and possession penalty are covered by:

```powershell
.\build\allstar_port.exe --test-one-on-one-shooting
```

`--test-all` includes this suite.

## ROM and Ghidra basis

The recovered bank-1 shooting cluster provides the structural basis for this change:

| ROM routine | Observed behavior | Native counterpart |
|---|---|---|
| `$7BE8` | Once per frame, subtracts `$000F` from 8.8 vertical velocity and integrates the three velocity/position pairs. | `allstar_physics_update_ball` uses a 60 Hz fixed step and the equivalent `15/256` gravity delta. |
| `$7C58` | Branches on the player's existing shot/jump state, constructs shot state, and transfers possession to the in-flight ball state. | Staged `allstar_one_on_one_shot_press` plus `one_on_one_launch_shot`. |
| `$7EA9` | For the normal shot vector, shifts signed target displacement left three bits. In 8.8 state this reaches the target in `256/8 = 32` frames. | `allstar_physics_shoot_ball` constructs a 32-frame target crossing. |
| `$791D/$794B` | Classifies player field `+$16` as 0, 1, or 2 from the exact left/right court wedge tables at `$79B6/$79D2`. | `allstar_one_on_one_rom_shot_variant` applies the recovered thresholds after converting native center X back to ROM field `+$06`. |
| `$7F37` | Chooses release offsets by action/facing while held and by shot phase/variant during the jump; phase 3 leaves the origin unchanged. | `allstar_one_on_one_rom_release_offset` contains the exact signed table and One-on-One launches consume its phase-two, position-selected variant result. |
| `$6945` animation update | Action `$0A/$12` advances through duration/frame records totaling 67 duration frames, then transitions to action `$0D`; non-shot actions clear shot phase. | The native shot pose has a finite 67-frame action clock, independent of whether the ball scores, returns, or reaches a dead boundary. |
| Fixed `$1CED` | Dispatches outer limits, back-court return, hoop/backboard contact, and ground/bounce behavior. | The exact outer-limit and `y<$5C` return branches are ported; later contact branches remain partial. |
| Fixed `$1F4D` | Zeroes the two planar 8.8 velocity words. | `allstar_physics_apply_rom_court_contacts` zeroes native `vx` and `vy`; deterministic boundary tests cover the semantic result. |
| Fixed `$077D` | Tests loose-ball proximity against player reference coordinates with strict Y `<8` and X `<12` limits. | `allstar_one_on_one_player_can_pick_up_ball` reproduces the strict limits and is boundary-tested. |
| Fixed `$2AE2/$2B07/$2B88` | Decrements the pickup cooldown, applies possession/global/height gates, tests player 1 then player 2 for action and collision eligibility, applies final contact/flight locks, and reloads a 20-frame cooldown on award. | `allstar_one_on_one_rom_recovery_dispatch` preserves that order and is boundary-tested; the scene supplies first-contact flight state and exact proximity, while three still-unclassified global locks remain inactive. |

The original control description also establishes that A begins the jump, A releases the shot, and landing with the ball is traveling. That relationship is implemented and deterministically tested.

## Known deviations

This is a **partial rules improvement**, not verified shot or physics parity:

- the native two-press/30-frame gather window is still a gameplay approximation; `$702D` input and `$C16A` phase timing need an emulator trace;
- the `$7F37` height offset still lacks the ROM player jump-height field (`+$05`), so native release `z` remains the existing release-height constant;
- the recovered fixed-step constants are represented with native floats rather than byte-identical 8.8 storage;
- the alternate `<<2` 64-frame trajectory class and `$7C58` launch tables are not yet classified;
- accuracy ratings, contests, blocks, the remaining `$1CED` rim/backboard/bounce branches, and full rebound gates are not trace-matched;
- CPU shot selection still comes from the generic native AI controller.

The former native trajectory aimed One-on-One shots at `z=112` and could reach that coordinate while rising, making its descending-only basket check incapable of recognizing the intended target crossing. The corrected hoop plane is `z=16`, shared by the other native shooting scenes, and basket detection now interpolates the descending plane crossing rather than accepting a broad 20-pixel height band.

The former fix treated native `y<76` and other reachability limits as an immediate out-of-bounds turnover. The traced ROM does not do that. `$1CED` treats `y<$5C` as a return contact, while only `x<$0A`, `x>=$A0`, or `y>=$97` call `$1F4D`; that helper stops planar motion without awarding possession. Possession is resolved later by `$2AE2/$2B07/$2B88` after `$077D` reports player contact. The native scene now follows that separation.

Native player X is a center coordinate corresponding to ROM field `+$06 + 8`; native Y corresponds to ground field `+$15`. Release X therefore subtracts that center bias after `$7F37`, while recovery compares the ball to native X and native Y minus two exactly as `$077D` does.

For the remaining deviations, `behavior.one_on_one_rules`, `behavior.physics`, `behavior.ai`, and `behavior.animation` remain partial, and this work adds no strict milestone credit.
