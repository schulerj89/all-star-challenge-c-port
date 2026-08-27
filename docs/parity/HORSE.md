# H-O-R-S-E parity path

Last verified: **2026-08-15**

The procedural `rand()%100` prototype has been removed. Native H-O-R-S-E is
now a dedicated mode-2 state machine in `allstar_horse.c` and
`scene_horse.c`. It reuses only the cartridge paths that mode 2 actually
shares with One-on-One: court/player art, `$702D/$6A8C` player control and
animation, `$7C58/$7EA9` launch, `$7BE8` flight, and `$1CED` rim/score
contacts.

The scoped gameplay manifest is **30/30 (100.00%)**. Under the stricter
whole-Ghidra-function metric, the 45 dedicated and shared routines used by
the mode are **28 verified, 17 candidate, 0 unmapped**: **62.22% verified**
and **100.00% mapped**. Candidate means the Horse branch is implemented but
the shared routine also owns other modes, VRAM timing, or platform-specific
work, so whole-function credit is intentionally withheld.

## Cartridge path translated to C

```text
bank 2 $4000
  <- fixed $22EF sees mode $02 and returns without a settings screen
  -> $4034 P1 roster selection
  -> mode 2 continues through the opponent selector (unlike Free Throw)
  -> $40F4 accepted-player command $0E

fixed $0CDF mode entry
  -> clear $DD73 (stop roster/title music)
  -> $10AD/$0444 shared player/game setup
  -> $22B9: P1 +$0E = 5, P2 +$0E = 5
  -> $04B1(A=1): One-on-One court
  -> $22C3 clears the two court-map colon cells through $053C
  -> $0749 draws both fixed-width player last-name fields
  -> bank 1 $7BA8/$06C0 draws letter prefixes with the ROM BG font
  -> bank 1 $7A90 active-shooter initialization

fixed $0D57 current-player turn
  caller ($C172 == $FFDA)
    -> bank 1 $7AEA
       -> P2 CPU caller uses $6CAB/$6DB7 spot table
    -> shared $100F/$702D control and $7C58 shot
    -> $0E36 saves aligned center/ground in $FFDB/$FFDC
    -> made: $C180=1 and caller remains
    -> missed: $C180=0 and caller toggles

  matcher ($C172 != $FFDA)
    -> bank 1 $7AFD approaches saved $FFDB/$FFDC
       -> $7B7A blinks blank tile $24 / X tile $76
    -> shared $100F/$702D control and $7C58 shot
    -> made: no letter
    -> missed: $0E26 decrements current player +$0E
       -> $7BA8/$7BC0 redraw incurred H/O/R/S/E prefix
       -> $2F88 command $07
       -> zero remaining ends the game
```

`allstar_horse_resolve_shot_0d57` owns caller/matcher rules and completion.
`allstar_horse_save_spot_0e36` performs the exact `AND $FC` alignment and the
special `$98 -> $94` lower-court adjustment. `allstar_horse_cpu_spot_6cab`
contains all 50 ROM pairs, selected by `$FFFB` thresholds `$30/$5E/$8C/$BA`
and cycled ten at a time.

## Measured timing and live proof

`trace_horse.lua` boots the original cartridge, selects menu ID `$02`, accepts
both roster players, and drives a deterministic called make followed by a CPU
matcher miss. The decisive checkpoints are:

| Event | Horse frame | State |
|---|---:|---|
| `$22EF -> $255D` settings bypass | before entry | mode `$02` loads no settings tilemap |
| P1 turn begins at `$0D57` | 247 | caller 1, letters 5/5 |
| P1 launches through `$7C58` | 319 | moved shooting location |
| `$1E0E` make | 381 | called spot becomes `$74/$88` |
| shared net command `$08` | 401 | make +20 |
| `$1ECC` net sequence | 401/416/431/446 | bend/deep/bend/rest at +20/+35/+50/+65 |
| shared score command `$05` | 446 | make +65 |
| P2 matcher begins at `$7AFD` | 691 | same `$74/$88` target |
| P2 launches | 762 | matcher at called location |
| `$0E26` letter | 942 | P2 5 -> 4 |
| next P1 turn | 1134 | HUD shows P2 `H` |

This proves a **180-frame release-to-result** interval for the CPU miss and a
**192-frame result-to-next-turn** interval. Native uses those counts directly.
The trace also captures exact live VRAM tile `$76`:

```text
00 00 00 00 C3 C3 66 66 3C 3C 3C 3C 66 66 C3 C3
```

It is already the final source tile (index 41) in the extracted 42-tile
ball/object stream, so no procedural X or new derived graphics were added.

The same capture records the first six live BG rows. `$22C3` calls `$053C`
twice to replace tile `$09` (the base court's `:` placeholder) with blank
tile `$00` at `(3,1)` and `(16,1)`. `$0749` then writes both nine-character
last-name fields at `(0,5)` and `(11,5)`, while `$7BA8->$06C0` converts
letters into the signed BG font range `$C0..$FA`. Native uses those exact
font tiles from the extracted bank-1 `$640F` stream instead of a generic PC
font.

The net trace also resolves presentation priority precisely. `$1E0E` gives
the made ball BG priority immediately; `$1ECC` leaves the court's original
`$14/$15,$1A/$1B,$23/$24` net untouched for 20 frames, then writes the
extracted bend/deep/bend/rest arrangements at +20/+35/+50/+65. Native now
keeps the ball behind the net through +34 and preserves that base frame
instead of substituting the final replacement before animation begins.

## Audio proof

`$0E26` selects command `$07`. The asset pack carries that exact focused
program to the existing `$3014` decoder:

| Command | Program | Priority | Stream | Frames | Instrument/register proof |
|---|---:|---:|---:|---:|---|
| `$07` Horse letter | `$06` | `$2A` | `$3EB6` | 42 | `NR10/11/12=$88/$40/$F2`; frequencies `$0783 -> $079D` |

The Mesen trace observes the first trigger as
`NR10/11/12/13/14=88/40/F2/83/BF`, then the second note writes frequency low
byte `$9D`. The decoder validates the command word, pointers, instrument,
duration expansion, both notes, and shared source FNV-1a `A0245071`.
The same trace also proves that a make keeps the shared `$1F26` command `$08`
at +20 and `$1F23` command `$05` at +65; the CPU matching miss reaches
command `$0A` at release +64. Native mode 2 dispatches those already-extracted
programs at the same events.

Export playable proof with:

```powershell
.\build\allstar_port.exe --export-horse-sfx `
  .\build\allstar.assetpack `
  .\build\proof\horse\horse_letter_command_07.wav
```

## Coverage accounting

| Ghidra scope | Verified | Candidate | Unmapped | Strict coverage |
|---|---:|---:|---:|---:|
| Dedicated Horse lifecycle/rules/presentation (14) | 10 | 4 | 0 | 71.43% |
| Shared selector/gameplay/render/audio dependencies (31) | 18 | 13 | 0 | 58.06% |
| **Mode total (45)** | **28** | **17** | **0** | **62.22%** |

Candidate examples are `$0CDF/$0D2B` (shared fade/VRAM lifecycle), `$7AFD`
(native matching behavior is exact but not a register-level controller-loop
clone), and `$7C58/$7BE8/$1CED/$3014` (the Horse path is covered while those
routines also serve other modes or audio programs). The scoped 30/30 metric
answers whether the playable Horse behavior requested here is present; the
62.22% metric answers whether every entire shared Ghidra routine qualifies for
strict whole-function credit.

## Verification

```powershell
.\build.ps1
.\build\allstar_port.exe --test-horse
.\build\allstar_port.exe --test-all
python tools\check_horse_coverage.py --require-min 100
.\tools\ghidra\run_ghidra_decomp.ps1
```

Native proof frames are generated as:

- `07_horse.bmp` — caller at mode start;
- `07a_horse_shot_flight.bmp` — shared shot animation/ball flight;
- `07b_horse_match_x.bmp` — CPU matching the saved X;
- `07c_horse_letter_h.bmp` — P2 letter after the matcher miss;
- `07d` through `07g_horse_net_*.bmp` — exact bend/deep/bend/rest sequence.

Generated screenshots, WAVs, ROMs, and asset packs remain ignored build
artifacts and are not shipped from the user-owned ROM.
