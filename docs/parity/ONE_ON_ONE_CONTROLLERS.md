# One-on-One `$702D` / `$7170` Controller Parity

Last reviewed: **2026-08-15**

## Result

The native One-on-One scene now follows the cartridge's actual controller
chain instead of maintaining separate human and shortcut CPU movement paths:

```text
$7170 CPU decisions -> $FFD2/$FFD3 synthetic input
                                |
human +$11/+$12 input ----------+
                                v
                    $7015 -> $702D -> $6A8C
```

`allstar_ai_rom_controller_7170` produces ROM-layout input bytes. Both players
then run `allstar_one_on_one_rom_player_controller_702d`; `$6A8C/$782E/$6B72`
remain the only path that selects records and applies movement. This removes
the former floating-point CPU movement shortcut and its artificial 8/4/1
decision delay.

## `$702D` path converted to C

| ROM path | Recovered state transition | Native implementation |
|---|---|---|
| `$702D->$0A78` | Split normal actions from protected shot/jump actions. | The shared controller calls the exact protected-action predicate first. |
| `$7034-$7059` | `$FFE7` or no high-nibble direction clears `+$07` and contact `+$0C`; otherwise the high nibble becomes `+$07` and stored `+$10`. | `input_direction`, `blocked_contact`, and `stored_direction` are updated in the same order. |
| `$7059->$70FD` | New A has priority over every B branch. Without the ball it selects jump `$05/$0C/$14`; with the ball `$0AA3` resets the record and selects gather `$0A/$12`. | Reset/direct-action events preserve the distinction between `$6C90` record resets and `$714D` direct assignments. |
| `$7074->$2B14` | New B while defending starts a steal only when `+$17` is clear. | The controller emits the steal event; the existing exact `$2B14->$077D->$2B88` path performs contact and transfer. |
| `$708A` | Held B with possession copies stored `+$10` into override `+$14` once; releasing B clears it. | `direction_override` is persistent controller state consumed by `$782E`. |
| `$709A->$714D->$7F37` | Protected actions retain direction. Non-jump shots directly select `$0A/$12`, preserve the current record, face the hoop, and refresh held-ball presentation. | Direct-action and ball-presentation events mirror those side effects without resetting the record. |
| `$70A3-$70F2` | Phase `+$13` plus global `$C16A` performs the one-update A-then-B release; new A releases phase zero. `$FFE3` blocks launch. | The native phase/latch transitions reproduce A→A and A→B, including phase 1→2 before `$7C58`. |
| `$70BF->$2B6C` | Protected defensive jumps retry the exact airborne recovery path on every controller call. | A recovery event feeds the existing `$2B6C/$2B88` catch logic while the jump action/direction remains intact. |

## `$7170` path converted to C

| ROM path | Recovered behavior | Native implementation |
|---|---|---|
| `$7170-$7190` | Clear `$FFD2/$FFD3`; gate counted waits, CPU enable, and score events; modes 0/4 use normal play, modes 1/3 return, mode 2 uses `$74A8`. | `AllStarRomCpuControllerContext` carries each gate and mode; outputs are cleared before every early return. |
| `$7190-$72B8` | Defense resets offense state, chases a loose ball, attempts skill-table steals, contests initial flight, chooses stored-direction offsets or alternate positioning, and consumes `$75CD`. | Exact `$7626/$7629/$762C/$762F` thresholds and byte-coordinate targets produce `$FFD2/$FFD3`. |
| `$72BF-$73C8` | Offense chooses `$72EA` side targets or `$732C` roster route families, then advances only on `$74BB` arrival. | All side/entropy bins, 27 roster keys, three route families, and 14 coordinate bins are represented directly. |
| `$73C9-$7474` | Fixed-center routes use the `$C0FF` A/B timer; ordinary offense can perform a 50-count B drive, reroute after contact, or continue to target. | The exact unsigned countdown and `$FFFD<$07` close-drive branch are persistent AI state. |
| `$7476-$7496` | Loose-ball arrival can press A when ball height and the `$7629` skill threshold qualify. | The CPU emits the same accepted-direction-or-A new input. |
| `$74BB-$755D` | Four-pixel asymmetric dead zones feed `$C103/$C144` 8/4/1 direction hysteresis. Arrival clears transient counters, changes the initial target to a route, then arms gather. | `allstar_ai_rom_target_74bb` restores prior `$C0FE` until the hysteresis counter expires and performs the same arrival transitions. |
| `$756C` | Stored profile entropy requires distance record 4/5/6/7; fallback requires records 5–7 and skill thresholds `$1A/$0C/$06`. Mode 2 bypasses the fallback skill comparison. | The native release decision reads the live `$6A8C` record and preserves the mode-2 bypass. |
| `$75CD` | Qualified body contact uses `$FFFB` against `$BE/$AA/$96`. Offense reroutes with independent `$FFFC` entropy until contact 14 arms A; defense saves its exact point and holds it for ten calls. | Qualification, independent reroute entropy, counter, and saved-point hold are integrated into the full controller. |

The values 8/4/1 written by fixed `$1FFA` are **direction-change hysteresis
reloads**, not AI update delays. Correcting that interpretation is why the CPU
now updates smoothly while still retaining the cartridge's difficulty-based
reaction lag when a target direction changes.

## Verification evidence

- `tools/emulator/trace_one_on_one_input.lua` proves `$702D` A→A and A→B
  phase/latch/release state on the original cartridge.
- `tools/emulator/trace_one_on_one_defense.lua` proves the `$702D` steal and
  protected-jump/recovery branches.
- `tools/emulator/trace_one_on_one_cpu_decision.lua` proves the original
  `$7170->$72EA->$732C->$74BB->$755D->$756C->$7C58` offense path.
- `tools/emulator/trace_one_on_one_cpu_controller_full.lua` proves loose-ball
  chase, skill steal, initial-flight contest, `$75CD` saved-position hold, and
  `$74BB` hysteresis. Its final result is:

```text
CPU_BRANCH scenario=1 new=00 held=90 ...
CPU_BRANCH scenario=2 new=02 held=00 ...
CPU_BRANCH scenario=3 new=91 held=00 ...
CPU_BRANCH scenario=4 new=00 held=00 hold=0A saved=50,70 ...
CPU_BRANCH scenario=5 new=00 held=20 hysteresis=02 accepted=20
TRACE PASSED: $7170 defense/chase/steal/contest/contact/hysteresis
```

`--test-one-on-one-shooting` adds deterministic branch fixtures for all
normal/protected `$702D` outcomes, mode-0 offense/defense `$7170` states,
entry gates, contact response, route/gather/release, and mode-2 behavior.
`--test-one-on-one-presentation` proves the live CPU reaches stage 2 and
releases, while a landed defender re-enters movement instead of freezing.

## Coverage change

| Metric | Before | After | Delta |
|---|---:|---:|---:|
| Reviewed Ghidra routines | 34/118 (28.81%) | 38/118 (32.20%) | **+3.39 points** |
| Strict project milestones | 10/25 (40.00%) | 11/25 (44.00%) | **+4.00 points** |

The four routine promotions are `$702D`, its `$714D` helper, `$7170`, and
`$74BB`. This is state/behavior parity for the recovered controller paths,
not a claim of frame-perfect whole-ROM synchronization.
