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
| `$72BF->$72EA`, tables `$731C/$7324` | Starts an offense once per possession and chooses a left/right first target from four random bins split at `$30/$70/$B0`, with hoop side split at X `$54`. | `allstar_ai_rom_offense_target_72ea` starts the explicit first-target controller stage instead of rerolling on every decision tick. |
| `$74BB->$751D->$732C`, tables `$738D/$763B` | Arrival at the first target selects one of three roster-specific route families using `$FFFD`, then one of fourteen coordinate pairs using `$FFFE` (or fixed `$54/$5D`). | `allstar_ai_rom_route_target_732c` carries all 27 roster keys, route families, entropy thresholds, and coordinate pairs; the live controller drives to this second target. |
| `$74BB` | Subtracts target from current as an 8-bit value. Nonnegative deltas `0..3` and wrapped negative deltas `-4..-1` are arrival; `+4` still requests negative-axis movement and `-5` requests positive-axis movement. | `allstar_ai_rom_direction_74bb` retains those asymmetric `+3/+4/-4/-5` boundaries. The stall was the separate native clamp: `$6BBA` permits raw X `$08->$04` (center `16->12`), but C had clamped back to 16, making target `$0C` unreachable. The court minimum is now the exact centered X 12. |
| `$755D->$702D/$714D->$756C`, tables `$7632/$7638` | At route arrival, A first enters action `$0A/$12`. Later `$756C` uses stored `$C141`, live `$FFFB`, and current animation record `+$03`; it requires records `4/5/6/7` by distance or the skill fallback `5..7`. | The CPU enters a real gather, advances the shared `$6A8C` records, and exposes a release edge only when `allstar_ai_rom_should_shoot_756c` accepts that current record. It no longer launches immediately from a manufactured distance index. |
| `$2F40` plus `$7C58` tables | Maps roster indices into profiles 0/1/2. The profile changes both CPU decision probability and the exact vertical launch table. | `allstar_ai_set_rom_profile` and `allstar_one_on_one_rom_shot_vertical_velocity` preserve the roster effect; the former generic 2PT/3PT percentage roll is no longer used by One-on-One. |
| `$71EE->$07B4` | While the opponent's initial-flight state remains set, presses the CPU contest input only inside the hoop-centered margin `$0E` rectangle: Y `<$6A`, center X `$47..$61`. `$1F5F` rim contact preserves that state; `$1E5B/$1E77` clears it on the first ground bounce. | `allstar_ai_rom_should_contest_71ee` gates the contest/jump state and is tested on every strict boundary; once `ball.recoverable` mirrors the ground-cleared `$FFF8`, the CPU leaves contest state and follows the rebound. |
| `$71B3`, table `$762C` | When the CPU touches the opponent's held ball, presses B if the random byte is below `$04/$19/$46` for skill 1/2/3. | `allstar_ai_rom_should_steal_71b3` is threshold-tested and feeds the shared `$2B14` steal path. |

The deterministic checks are part of:

```powershell
.\build\allstar_port.exe --test-one-on-one-shooting
```

Live Mesen proof is in `tools/emulator/trace_one_on_one_cpu_decision.lua`; the
deterministic capture drives through `$40/$98` and `$24/$64`, gathers for 25
frames, and releases on record `$05`. The broader `$7170` controller includes collision and other-mode
branches not claimed here. `$74BB`, `$78E9`, and `$7170` therefore remain
candidate whole-routine mappings in the Ghidra inventory even though these
scoped One-on-One requirements receive behavior-manifest credit.

The native scene regression in `--test-one-on-one-presentation` additionally
starts a real CPU possession, crosses both route stages, and observes a live
ball release at scene frame 130. This is the integration proof that caught the
`$6BBA` final edge-step/clamp mismatch which isolated target-table tests had
missed.
