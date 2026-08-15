# One-on-One CPU and Scoring Evidence

Last reviewed: **2026-08-15**

This checkpoint replaces the generic six-state decisions used by the native
One-on-One scene with the recovered mode-0 selectors from bank 1. The claim is
limited to positioning targets, shot decisions, roster-profile effects,
skill-based steal input, and the shot-contest gate. The resulting steal and
defensive-jump behavior is documented in `ONE_ON_ONE_DEFENSE.md`.

| ROM control flow | Recovered behavior | Native counterpart |
|---|---|---|
| `$78E9->$794B`, table `$798B` | Tests the held ball against fourteen Y/X rows. Lower X is strict, upper X inclusive. The result is inverted into `$FFD6`; `$7C58/$7F0A` copies it to `$FFD7`, and `$1F23` awards 2 for zero or 3 otherwise. | `allstar_one_on_one_rom_point_value` is used at the recovered release origin instead of Euclidean distance. Tests cover lower, upper, tapered, and beyond-table boundaries. |
| `$72EA`, tables `$731C/$7324` | Chooses left/right offensive targets from four random bins split at `$30/$70/$B0`, with hoop side split at X `$54`. | `allstar_ai_rom_offense_target_72ea` stores the exact target bytes in the live CPU controller. |
| `$74BB` | Emits axis direction bits only outside the asymmetric four-pixel unsigned dead zone. | `allstar_ai_rom_direction_74bb` drives offensive and defensive movement; exact `+3/+4/-4/-5` boundaries are tested. |
| `$756C`, tables `$7632/$7638` | Uses roster profile thresholds `$B0/$60/$40`, requires distance actions `4/5/6/7`, then falls back to difficulty thresholds `$1A/$0C/$06` for actions 5..7. | `allstar_ai_rom_should_shoot_756c` is called on each difficulty-cadenced CPU decision and is threshold-tested. |
| `$2F40` plus `$7C58` tables | Maps roster indices into profiles 0/1/2. The profile changes both CPU decision probability and the exact vertical launch table. | `allstar_ai_set_rom_profile` and `allstar_one_on_one_rom_shot_vertical_velocity` preserve the roster effect; the former generic 2PT/3PT percentage roll is no longer used by One-on-One. |
| `$71EE->$07B4` | While an opponent shot is in flight, presses the CPU contest input only inside the hoop-centered margin `$0E` rectangle: Y `<$6A`, center X `$47..$61`. | `allstar_ai_rom_should_contest_71ee` gates the contest/jump state and is tested on every strict boundary. |
| `$71B3`, table `$762C` | When the CPU touches the opponent's held ball, presses B if the random byte is below `$04/$19/$46` for skill 1/2/3. | `allstar_ai_rom_should_steal_71b3` is threshold-tested and feeds the shared `$2B14` steal path. |

The deterministic checks are part of:

```powershell
.\build\allstar_port.exe --test-one-on-one-shooting
```

The broader `$7170` controller includes collision and other-mode
branches not claimed here. `$74BB`, `$78E9`, and `$7170` therefore remain
candidate whole-routine mappings in the Ghidra inventory even though these
scoped One-on-One requirements receive behavior-manifest credit.
