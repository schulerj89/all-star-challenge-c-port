# Free Throw Ghidra-to-C conversion

Last verified: **2026-08-15**

## Result and scope

Free Throws is now a playable native mode: D-pad aim, A release, ROM 8.8
flight, rim/make outcomes, animated net, delayed score, 5/10/20 attempts,
results, and return to the title flow are implemented. The scoped behavior
manifest is **20/20 (100.00%)**. Exact bank-3 background tiles and the original
four-entry ball OAM tile table remain presentation work and are not counted in
that behavioral denominator.

## Correct dispatcher path

The fixed-bank mode table at `$0267` maps mode `$01` to `$0C8E`. The nearby
`$0CDF` entry is H-O-R-S-E; it is not Free Throws. The traced Free Throw path is:

```text
$0C8E mode lifecycle
  -> $17AA attempt reset
     -> $18E7 four-row RNG aim seed
  -> $100F mode-1 frame branch
     -> $1C1D sprite priority
     -> $1884 ball OAM position
     -> $1C61 net/score timer
     -> $1942 held D-pad aim input
     -> $1986 8.8 reticle integration
     -> $1CAA new-A release
        -> bank 1 $7C58/$7EA9 launch vector
     -> bank 1 $7BE8 fixed-point ball integration
     -> $1A31 Free Throw rim/outcome dispatcher
     -> $1A25 previous-reticle position
  -> $17E2 between-attempt presentation
  -> next $17AA or shared result flow
```

Native code is split accordingly between
`include/allstar_free_throw.h`, `src/gameplay/allstar_free_throw.c`, and
`src/scenes/scene_free_throw.c`. The scene owns input, audio event dispatch,
drawing, results, and exit; the gameplay file owns the translated ROM state.

## Recovered behavior

`$17AA` resets the ball integer coordinates to X/Y/Z `$4F/$E0/$72`, the net
to state 7 with timer `$10`, and calls `$18E7`. `$18E7` selects one of four
exact target/velocity rows from `$FFFB` thresholds `$40/$90/$E0`.

`$1942` changes horizontal or vertical aim velocity by one raw 8.8 unit for
each held direction. `$1986` attracts the target toward X `$50` and Y `$3F`,
applies its six-count acceleration cadence, integrates both axes, and stops
velocity outside X `$39..$68` or Y `$28..$57`.

`$1CAA->$7C58` releases only on a new A edge. A 19/256 assist applies when the
target is already inside X `$48..$57`, Y `$36..$43`; its working target snaps
to `$52/$3C` without changing the displayed reticle. The launch equations are:

```text
VX = (target_x - 12 - ball_x) << 2
VY = (0xB8 - ball_y) << 2
VZ = 0x01F4 + ((0xB8 - (target_y + 0x0E) - ball_z) << 2)
```

The live center target therefore produces `(-36, -160, 484)`, exactly matched
by the native regression.

`$17E2` places the presentation ball at X `$50`, Z `$79`, runs the frozen
preflight, adds three to Z, enables `$7BE8`, then performs its `$B4 + $64`
controller calls. The next `$17AA` occurs 292 traced frames after release.

## Rim and made-shot path

`$1A31` first clamps a ball that crosses Y `$B8` to `$BA`, assigns VY `$20`,
normalizes nonzero VX to `+/-$24`, and emits command `$0A`. For Z `$77..$79`
and X `$33..$55`, table `$1B0F` selects one of eleven handlers.

For the deterministic center shot, Mesen proves this exact sequence:

```text
release +77   $1B59  X=$46, C164 0->1
release +92   $1B93  X=$3E -> $1E74 rim bounce
release +97   $1B53  X=$40
release +113  $1B99  X=$4A -> $1E74 rim bounce
release +118  $1B59  X=$48, C164 1->2
               -> subtract 3 from X -> $1C05 -> $1E0E make
```

The Free Throw bounce subtracts `$0039` after negating VZ. Using the harder
One-on-One bounce value is incorrect and prevents this path from returning to
the second `$1B59` make cell.

`$1C61` advances the seven-frame net table with an initial 16-count followed
by 11-count intervals. It emits command `$08` at state 2 (make +27), then
command `$05` and increments the selected score at state 6 (make +71). The
native center trace consequently reports make `118`, `$08` at `145`, `$05`
at `189`, and next attempt at `292`.

## Audio proof

The expanded version-12 asset pack decodes two mode-specific `$3014` programs
directly from the user-owned ROM:

| Command | Program | Priority | Stream | Frames | First live APU state |
|---|---:|---:|---:|---:|---|
| `$08` net | `$05` | `$23` | `$3EAC` | 57 | `NR41/42/43/44=C6/F1/10/A0` |
| `$0A` ball contact | `$0D` | `$1C` | `$3F0A` | 12 | `NR10/11/12=FF/7F/F1` |

`trace_free_throw.lua` captures those writes from the original cartridge.
The native pack validator checks the command table words, stream pointers,
instruments, duration expansion, channel registers, and shared source FNV-1a
`A0245071`. Export WAV proof with:

```powershell
.\build\allstar_port.exe --export-free-throw-sfx `
  .\build\allstar.assetpack `
  .\build\free_throw_net_08.wav `
  .\build\free_throw_contact_0A.wav
```

## Verification

```powershell
.\build\allstar_port.exe --test-free-throw
python tools/check_free_throw_coverage.py --require-min 100
```

The native test asserts all four RNG rows, the center launch vector, live
make/net/score timing, five successful attempts, result state, and A-to-title
exit. `tools/emulator/trace_free_throw.lua` independently boots the original
ROM through its real menu and proves the same path and APU commands.

Generated visual proof is written under `build/free_throw_proof/`:

- `06_free_throw.bmp` — live reticle and ready ball;
- `06a_free_throw_flight.bmp` — fixed-point flight;
- `06b_free_throw_make.bmp` — made-ball/net priority;
- `06c_free_throw_net.bmp` — animated net phase;
- `06d_free_throw_result.bmp` — completed 5/5 result screen.
