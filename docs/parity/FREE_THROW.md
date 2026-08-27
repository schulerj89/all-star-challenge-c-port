# Free Throw Ghidra-to-C conversion

Last verified: **2026-08-15**

## Result and scope

Free Throws is now a playable native mode: D-pad aim, A release, ROM 8.8
flight, rim/make outcomes, animated net, delayed score, 5/10/20 attempts,
results, and return to the title flow are implemented. Free Throw selection
now accepts one player and enters the mode without an opponent or VS card,
and mode entry stops the title/menu song. The earlier procedural
top-down court was incorrect. The ROM actually presents a close-up fixed
shooter/backboard scene. Asset-pack v14 extracts and draws that mode's
separate graphics. The expanded gameplay-and-presentation manifest is
**32/32 (100.00%)**. Fixed `$1C1D` now consumes the prior `$1884` OAM Y,
including its `$58` threshold and the mode-1 `$C12B=$2D` made-ball hold.

## Correct dispatcher path

The fixed-bank mode table at `$0267` maps mode `$01` to `$0C8E`. Before that,
bank-2 `$4000` sees one player at `$FF91=1` and mode `$FF8F=1`; its
`$4014-$401D` branch jumps directly to `$4034`, which runs only the first
player selector and returns. It never enters `$4023`, the opponent selector,
or any VS presentation. The nearby
`$0CDF` entry is H-O-R-S-E; it is not Free Throws. The traced Free Throw path is:

```text
$0C8E mode lifecycle
  -> clear $DD73 (stop prior music command)
  -> $2243 copies fixed $22A9 reticle art to OBJ tile $7F
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
`src/scenes/scene_free_throw.c`. Asset extraction is in
`src/allstar_asset_pack.c`. The scene owns input, audio event dispatch,
drawing, results, and exit; the gameplay file owns the translated ROM state.

## Recovered mode-specific graphics

The Free Throw screen does not use the One-on-One court or player-frame
renderer. The exact `$0C8E->$2243/$1CBD` loads are:

| ROM path | Destination/use | Decoded bytes | FNV-1a |
|---|---|---:|---:|
| bank 1 `$640F..$6660` via `$050F` | VRAM `$8C00`; signed BG font IDs `$C0..$FA` | 944 | `EA6C7CAD` |
| bank 3 `$6EF1..$708D` via `$050F` | VRAM `$8000`; 30 OBJ tiles for `$1884` | 480 | `ADFC1015` |
| bank 3 `$708E..$793E` via `$050F` | VRAM `$9000`, wrapping at `$9800` to `$8800`; BG IDs `$00..$A2` | 2,608 | `B088FF84` |
| bank 3 `$7F69..$7FCC` via `$050F` | VRAM `$9800`; 32x32 base tilemap | 1,024 | `3501C85B` |
| fixed bank `$22A9..$22B8` via `$0496` | VRAM `$87F0`; OBJ tile `$7F` aiming reticle | 16 | exact literal copy |

Fixed `$1828` selects three 9x8 close-up shooter maps at bank-1
`$6661/$66A9/$66F1`. Fixed `$1858` selects four 3x5 hoop/net maps at
`$6739/$6748/$6757/$6766`. Fixed `$1884` selects one of three 4x4 OBJ maps
at bank-3 `$7FCD/$7FDD/$7FED` from ball Y thresholds `$C4/$D2`, then emits
all 16 OAM entries. `$0749->$06C0` writes the selected player's last name;
the shooter graphics themselves are fixed mode art, not per-roster sheets.

The reticle is not a procedural crosshair and it is not part of the
background. `$2243` initializes OAM entry `$C098..$C09B` to
`{Y=0,X=0,tile=$7F,attr=0}` after copying the fixed 16 bytes at `$22A9` to
VRAM `$87F0`. Each mode-1 frame calls `$1A25` after `$1942/$1986`; it writes
the integer target Y to `$C098` and X to `$C099`. Native rendering uses the
same raw OAM conversion (`screen_y=Y-16`, `screen_x=X-8`) and exact tile.
Mesen observed `{Y=$33,X=$48,tile=$7F,attr=$00}` on the first live update.

`$100F` calls `$1C1D` before `$1884`. Native therefore snapshots the prior
OAM Y for all four sprite rows, applies the exact `<$58`/`>=$58` priority
decision, then captures the new 16-entry `$1884` position before physics.
On a make, `$1E49` writes `$C12B=$2D`; native holds gravity and forces all
four rows behind nonzero BG pixels for the same countdown.

The final screen is also cartridge-derived: selected last name on row 4,
`SHOTS ATTEMPTED` on row 7, and `SHOTS MADE` on row 10. The former native
boxed `FREE THROWS / SCORE n / n` panel was not a ROM screen and was removed
from the asset-backed path.

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

Before that eleven-way rim dispatch, `$1A7E->$1AA6` compares the captured
release target against X table `$1AAD = {4F,50,51}` and a shooter-profile Y
list selected through `$1AA7`:

| `$2F40` profile | ROM Y list | Clean cells |
|---:|---|---:|
| 0 | `$38..$3E` | 21 |
| 1 | `$39..$3D` | 15 |
| 2 | `$3B..$3C` | 6 |

A match jumps directly to `$1C05->$1E0E`; it does not need the later
double-rim route. `trace_free_throw_make_window.lua` proves roster `$00`
(profile 2), target `$50/$3B`, and the exact
`$1A7E->$1A94->$1C05` make at release +77. The native exhaustive regression
checks all 224 visible assist-rectangle cells for each profile and obtains
the ROM counts 21/15/6. `$1BBD` also accepts its separate X `$4F..$51`, Y
`$32/$33` center-rim override before consulting the profile latch.

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

`$1E0E` seeds `$C12B=$2D`; `$1C1D` therefore applies OBJ priority to every
ball row during the made-shot/net passage. The native software compositor
uses that live timer immediately, keeping the ball behind nonzero foreground
net/background pixels even on the first rendered make frame.

## Audio proof

The mode has no replacement background song. Fixed `$0C8E` begins with
`xor a; ld [$DD73],a`; `$3014` treats zero as no active music program. Native
mode entry therefore stops the roster/title BGM rather than letting it loop
under Free Throw gameplay. The Mesen trace reaches `$0C92` with `$DD73=$00`.

The asset pack decodes two mode-specific `$3014` programs
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

The native test asserts the single-player/no-VS transition, `$0C8E` music
stop, all four RNG rows, reticle state, center launch vector, live
make/net/score timing, five successful attempts, result state, and A-to-title
exit. `tools/emulator/trace_free_throw.lua` independently boots the original
ROM through its real menu and proves the `$4018->$4034` selector branch,
`$DD73=0`, OAM tile `$7F`, shot path, and APU commands.

Generated native visual proof is written under
`build/free_throw_reticle_proof/`.
Original Mesen captures are `build/original_free_throw_aim_png.png`,
`build/original_free_throw_flight_plus60.png`, and
`build/original_free_throw_result_png.png`. These generated files are ignored
and are not shipped as repository assets.

The native proof set contains:

- `03a_free_throw_roster.bmp` — the only player card selected for mode `$01`;
- `03b_free_throw_no_vs.bmp` — the immediate next native frame is gameplay,
  not a VS card;
- `06_free_throw.bmp` — live exact-tile reticle and ready ball;
- `06e_free_throw_reticle_left.bmp` / `06f_free_throw_reticle_right.bmp` —
  exact `$1A25` OAM tile at two timing positions;
- `06a_free_throw_flight.bmp` — fixed-point flight;
- `06b_free_throw_make.bmp` — made-ball/net priority;
- `06c_free_throw_net.bmp` — animated net phase;
- `06d_free_throw_result.bmp` — completed 5/5 result screen.
