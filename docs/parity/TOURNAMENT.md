# Tournament Progression Evidence

Last reviewed: **2026-08-14**

## Verified scope

The native tournament controller now enforces the complete seven-match elimination flow tracked by `behavior.tournament`:

| Stage | Required state transition |
|---|---|
| Quarterfinals | Four adjacent seed pairs produce four semifinalists in bracket order. |
| Semifinals | Two adjacent semifinal pairs produce two finalists. |
| Final | The final winner becomes champion and locks the completed bracket. |
| Match return | A One-on-One result records only a player from the active pairing, clears `match_in_progress`, and returns to the bracket. |
| Champion exit | A or Start dismisses the completed bracket and returns to the title flow. |

The fixed-bank tournament controller at `$0F2E..$0FBA` repeatedly launches the One-on-One loop, consumes its winner, and advances through four first-round matches, two second-round matches, and one championship match. The recovered postgame path at `$10A5` feeds the same winner comparison used by the native One-on-One return route.

`allstar_tournament_record_winner` rejects inactive brackets, duplicate results, and player IDs outside the current pairing. This prevents a stale One-on-One result from mutating a later bracket slot.

Run:

```powershell
.\build\allstar_port.exe --test-tournament
```

The suite executes every transition, checks all pairings and output arrays, rejects an invalid winner, verifies the champion lock, and exercises the champion-to-title scene exit.

## Boundary

This milestone covers winner propagation, rounds, persistent bracket state, and championship completion. Roster-selection presentation and pixel-level bracket rendering are presentation/data tasks and do not receive tournament-gameplay credit here. No individual Ghidra routine mapping is promoted by this milestone; routine mapping remains separately evidence-gated.

## ROM-backed tournament driver

Added 2026-08-26. The section above tracks the native bracket helper in
`allstar_game.c`, whose shape was invented. This section tracks the port of the
cartridge's own driver, which uses a different RAM layout and a different
winner-propagation rule.

Mode `$04` in the `$0267` dispatch table enters `$0F2E`. The bracket lives in one
contiguous block that the ROM walks with pointer arithmetic, so
`include/allstar_tournament.h` models it at its real addresses:

| Address | Contents |
|---|---|
| `$C0BE` | match counter, 1-based while a match runs |
| `$C0BF..$C0C2` / `$C0C3..$C0C6` | round 1 left / right entrants |
| `$C0C7..$C0CA` | round 1 winners |
| `$C0CB..$C0CC` / `$C0CD..$C0CE` | round 2 left / right entrants |
| `$C0CF..$C0D0` | round 2 winners |
| `$C0D1` / `$C0D2` | final entrants |
| `$C0D4`/`$C0D5`, `$C0D6`/`$C0D7` | stage and banked win counters |
| `$C0D8..$C0F4` | `$FF`-bounded candidate list for the bank 2 selector |

Behaviour the disassembly establishes and the suite pins:

- The driver runs four matches, breaks at `$C0BE == $04`, runs two, breaks at
  `$C0BE == $06`, then runs one. `$C0BE` is incremented *before* `$0B80`.
- `$284D` credits `$C0D4`/`$C0D5` and copies the surviving entrant to `$C0C6 + N`,
  reading it from `$C0BE + N` or `$C0C2 + N` depending on which side won.
- `$286E` writes `$C0CF` on match `$05` and `$C0D0` on match `$06`.
- A tied `$28E1` result advances nothing.
- **The final calls neither `$10A5` nor any advance routine.** `$0F2E` returns
  straight after `$0B80`, so the driver never records a champion into the block.
- `$0B35` fills `$C0D9..$C0F3` with roster ids 0..26 between `$FF` sentinels.

Not yet ported: the bank 2 selector at `$4000` that consumes the candidate list
and writes the entrant slots. `$2890`, `$2897`, and `$28B0` therefore raise a
recorded selection request instead of filling those slots themselves.

### Postgame dispatch (chunk 3)

`$0F63` and `$0F91` call `$10A5`, which sets `$C16F`, puts screen `0` in `$FF8D`
and falls into the shared tail at `$10B7`. That tail pages in bank 1, loads
`$640F` into `$8C00` **unless the mode is `$01`**, then dispatches twice:

| `$FF8D` | `$10D9` target | | mode `$FF8F` | `$10E4` target |
|---|---|---|---|---|
| 0 | `$10E1`, dispatches again on the mode | | 0 One-on-One | `$10EE` |
| 1 | `$1343` | | 1 Free Throw | `$1121` |
| 2 | `$139B`, reached from `$28D9` | | 2 H-O-R-S-E | `$11D5` |
| 3 | `$146F` | | 3 Accuracy | `$1209` |
| | | | 4 Tournament | `$12A6` |

So a tournament match's postgame screen is `$12A6`, not the One-on-One `$10EE`.

`$1726` converts a BCD score word — hundreds in H's low nibble, tens and units in
L — into three tile codes at `$C1FB`, digit + `$C1`. A leading zero blanks, but a
zero tens digit prints as `0` once the hundreds digit has printed, and the units
digit always prints. `$1638` holds the screen until Start is newly pressed, which
`$FFEC` can suppress, or until the BC frame counter expires.

### Mode 1 and 2 postgame screens (chunk 4)

`$1121` (Free Throw) is a **link-cable handshake**, not just a result screen.
Each side writes `$F0` into its own score high byte and spins on the other
side's until it reads `$F0`:

| `$C199` | role | flags | polls | announce sound |
|---|---|---|---|---|
| `$00` | solo | none | none | none |
| `$01`/`$02` | player 1 | `$C134` | `$C136` | `$19` |
| `$03` | player 2 | `$C136` | `$C134` | `$18` |

If the other side has already flagged, `$1152` sets this side's flag and skips
the wait. The screen then draws the local player's name at `$0504`, the `$FF98`
attempt count at `$1107`, and the local score at `$110A`.

`$11D5` (H-O-R-S-E) picks a name with `$FFAB`, records the *other* player in
`$C17D` as the survivor, copies the name through `$170D`, appends the six bytes
at `$1203` (`"IS OUT"`, last byte OR `$80`), and draws the result at `$0207`.

`$170D` is the ROM's string tidier: copy until a byte has bit 7 set, walk back
clearing that bit and dropping trailing spaces, then append exactly one space.
`$1786` clears sixteen tiles per row, six by six, from `$9800`.

### Mode 3 and 4 postgame screens (chunk 5)

`$12A6` is where tournament match results are resolved, and it changes what the
bracket means.

| `$C0BE` | `$28E1` | path |
|---|---|---|
| 1..6 | decided | `$10FA` panel, then return to `$0F2E` |
| 1..6 | tied | **replay the match** |
| `$07` | tied | `$C192 = 1`, `$10FA` panel, then replay |
| `$07` | decided | champion screen at `$12FB` — `$10FA` is *not* drawn |

**A tie does not advance the bracket; it replays the match.** `$12C9` sets
`$C170`, plays sound `$16`, holds for `$00F0` frames through `$1638`, saves and
forces `$FF95`, clears the music, calls `$0B9A` — the One-on-One match body —
and then re-enters the dispatch at `$10A5`. That is the reason `$284D` and
`$286E` return early on a tied verdict: the tie is resolved by replaying, so
there is never a winner to record.

The champion path reads `$FFAC` or `$FFC5` by the `$28E1` verdict, loads that
player's record through bank 2 `$2DD2`, draws a field at record + `$16` at
`$0107`, then walks the record to the surname — skip spaces from offset 1, step
once more, step twice past a `.`, then back up one — and draws it at `$0507`
before setting the `$8E` champion music.

`$1209` (Accuracy) runs the same `$F0` handshake but gated on `$FF91 != $01`,
with no `$1152` shortcut: it always flags and always polls. `$1277` stores
whatever was in A, which is still the `$F0` it just wrote.

### VS and bracket screens (chunk 6)

`$1343` is the pre-match screen behind `$FF8D == $01`. Every mode gets both
names at `$0505`/`$0509` with `"VS"` (`$1399`) between them at `$0807`; only
`$FF8F == $04` adds the `"GAME"` string (`$1394`) at `$060D` and the match
number at `$0C0D`. That number is `$C0BE + $C1` — the same digit base `$1726`
uses, so match 1 draws tile `$C2`.

`$139B` is the bracket display, reached from `$28D9` after the bank 2 selector.
`$C17F` picks the stage:

| `$C17F` | shows | sound | first row |
|---|---|---|---|
| `$01` | nothing, returns at `$13AA` | — | — |
| `$02` | four semifinalists | `$0E` | 5 |
| `$03` | four semifinalists | `$00` | 5 |
| `$04` | all eight entrants | `$0D` | 1 |

Both lists **interleave the two sides** — `$C0BF`, `$C0C3`, `$C0C0`, `$C0C4`, …
— so each match pair lands on adjacent rows, two rows apart, in column 1. Every
slot goes through `$1464`, which writes the id to `$FFF6` and calls bank 2
`$2DD2` to load the record. The screen then holds for `$0384` frames.

### Bracket chooser (chunk 7)

`$146F` dispatches on `$C181` into four entry points that share one body. Each
picks a bracket list end in HL and a flag in B:

| entry | HL | B | one-player sound |
|---|---|---|---|
| `$1483` | `$C0C2` if `$C184 == 1`, else `$C0C6` | 0 | `$0F` |
| `$14B6` | `$C0C6` | 0 | `$10` |
| `$1493` | `$C0CC` if `$C184 == 1`, else `$C0CE` | 1 | `$0F` |
| `$14BD` | `$C0CE` | 1 | `$10` |

With two players the sound is `$11` or `$12` by `$C184` instead. B decides the
list length: zero draws four names in rows 10, 8, 6, 4; one draws the pair in
rows 6 and 4 — always column 3, and always **walking HL backwards**, so the
list reads bottom-up.

The body then runs a two-option toggle. `$1521` reads player 1's new input
unless this is a two-player game where `$C184` says player 2 is choosing, and
`$1533` masks it with `$CB`. Start (`$08`) confirms and returns `$C181`;
any other accepted bit flips `$C181` between 0 and 1, redraws through `$1554`
and blips `$2AB5`. `$1554` draws two three-row boxes at column `$0B`, the
selected one at row `$0C` and the other at row `$0F`, swapping the middle-row
art so position and highlight move together.

Run:

```powershell
.\build\allstar_port.exe --test-tournament-rom
.\build\allstar_port.exe --test-postgame-rom
.\build\allstar_port.exe --test-postgame-screens
.\build\allstar_port.exe --test-postgame-modes
.\build\allstar_port.exe --test-postgame-bracket
.\build\allstar_port.exe --test-postgame-chooser
python tools\check_tournament_rom_coverage.py
```

Progress against the 64 tournament-exclusive routines (2,961 bytes) is tracked in
`TOURNAMENT_ROM_COVERAGE.json`; the checker fails if a routine is marked ported
without a source file that actually references its address.
