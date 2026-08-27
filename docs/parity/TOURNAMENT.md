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

Run:

```powershell
.\build\allstar_port.exe --test-tournament-rom
python tools\check_tournament_rom_coverage.py
```

Progress against the 64 tournament-exclusive routines (2,961 bytes) is tracked in
`TOURNAMENT_ROM_COVERAGE.json`; the checker fails if a routine is marked ported
without a source file that actually references its address.
