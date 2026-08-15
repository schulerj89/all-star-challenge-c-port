# One-on-One Lifecycle Parity Evidence

Last verified: **2026-08-14**

## Scope

This checkpoint covers the One-on-One match lifecycle: initialization, timed and score-target endings, shot-clock turnover, final-score presentation, tied-game continuation, ordinary exit, and return of a tournament winner to the bracket. It does not claim parity for steals, blocks, rebounds, collision, shot contest, winners-outs, difficulty, AI, physics, or presentation.

## ROM control flow

The fixed bank contains the complete high-level lifecycle:

| ROM address | Observed responsibility | Native counterpart |
|---|---|---|
| `$0267` mode table | Mode ID `0` dispatches to `$0B80`. | `ALLSTAR_MODE_ONE_ON_ONE` routes to `ALLSTAR_SCENE_ONE_ON_ONE`. |
| `$0B80` | Clears both 16-bit scores and enters match setup. | `allstar_one_on_one_match_init`. |
| `$0B9A` | Loads the configured game clock and enters the gameplay loop. | Match `game_clock`, `period_seconds`, and scene update. |
| `$0BD7` / `$0FDE` | Detects a zero game clock and leaves live play. | `allstar_one_on_one_match_tick`. |
| `$0C00` | Compares both scores with the configured `$FF92` play-to value and ends when either reaches it. | `allstar_one_on_one_match_add_score`. |
| `$10A5` / `$10E1` / `$10EE` | Selects the mode-0 postgame path. | Result phase in `scene_one_on_one.c`. |
| `$28E1` | Compares the two 16-bit scores and returns player 1, player 2, or tie. | Match `winner` calculation. |
| `$10FA` | Draws both players and final scores. | `one_on_one_draw_result`. |
| `$15DF..$162E` | Holds the result for at most `$03C0` frames and permits A/B dismissal. | 16-second `result_clock` and A/B/Start dismissal. |
| `$12C9..$12F8` | On a tie, starts another period through `$0B9A` without clearing the accumulated scores. | `ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME`. |
| `$0F2E..$0FBA` | Tournament controller repeatedly launches One-on-One, records winners, and advances through four quarterfinals, two semifinals, and one final. | Persistent `AllStarTournamentState` and tournament return route. |

The ordinary non-tied ROM path does **not** present a replay menu. The final-score screen is skippable, then control returns to the title initialization path. Replay requires starting another match from the title/menu flow. The native port now follows that behavior instead of inventing a replay choice.

The original ROM was also observed running with the default `02:00` game clock and `24`-second shot clock. Those remain the native defaults until the separate settings-persistence milestone is completed.

## Deterministic verification

Run:

```powershell
.\build\allstar_port.exe --test-one-on-one-lifecycle
```

The test checks:

- 24-second expiration resets the shot clock and switches possession;
- a tied regulation result preserves both scores and starts period 2;
- a later non-tied result records the correct winner and completes;
- a configured play-to value ends immediately at the target score;
- an ordinary result returns to the title scene;
- a tournament result stores the winner, clears the in-progress flag, advances the bracket, and returns to the tournament scene.

`--test-all` includes this suite. The remaining One-on-One rules are tracked by `behavior.one_on_one_rules` and receive no credit from this lifecycle checkpoint.
