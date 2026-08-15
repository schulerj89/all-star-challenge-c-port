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
