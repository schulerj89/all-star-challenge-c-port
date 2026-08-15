# One-on-One Lifecycle Parity Evidence

Last verified: **2026-08-14**

## Scope

This checkpoint covers the One-on-One match lifecycle: initialization, timed and score-target endings, shot-clock turnover, final-score presentation, tied-game continuation, ordinary exit, and return of a tournament winner to the bracket. Possession and winners-outs work is documented separately in `ONE_ON_ONE_POSSESSION.md`; this lifecycle checkpoint does not claim parity for steals, blocks, collision, shot contest, AI, physics, or presentation.

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
| `$28E1` | Compares the two unsigned 16-bit scores and returns player 1, player 2, or tie. | Verified `allstar_one_on_one_compare_scores`. |
| `$10FA` | Draws both players and final scores. | `one_on_one_draw_result`. |
| `$15DF..$162E` | Holds the result for at most `$03C0` (960) frames and permits A/B dismissal through the `$0C` input mask. | 16-second result phase and exact A/B dismissal; Start does not dismiss it. |
| `$12C9..$12F8` | On a tie, shows a second overtime notice, calls `$1638` with `$00F0`, then re-enters `$0B9A` without clearing scores. | Separate `ALLSTAR_ONE_ON_ONE_OVERTIME` phase followed by period 2 with scores preserved. |
| `$1638` | Waits for at most 240 frames and permits only A through the `$08` input mask. | Four-second overtime notice with A-only dismissal. |
| `$0F2E..$0FBA` | Tournament controller repeatedly launches One-on-One, records winners, and advances through four quarterfinals, two semifinals, and one final. | Persistent `AllStarTournamentState` and tournament return route. |

The ordinary non-tied ROM path does **not** present a replay menu. The final-score screen is skippable, then control returns to the title initialization path. Replay requires starting another match from the title/menu flow. The native port now follows that behavior instead of inventing a replay choice.

The original ROM was also observed running with the default `02:00` game clock and `24`-second shot clock. The verified settings path now replaces the game-clock default with the selected `2`, `5`, `8`, or `12` minutes while retaining the 24-second shot clock.

## Deterministic verification

Run:

```powershell
.\build\allstar_port.exe --test-one-on-one-lifecycle
```

The test checks:

- 24-second expiration resets the shot clock and switches possession;
- the `$28E1` score comparator handles ties and 16-bit high-byte boundaries;
- final-score dismissal accepts A/B but not Start;
- a tied regulation result preserves both scores, enters the separate four-second/A-only overtime notice, and then starts period 2;
- a later non-tied result records the correct winner and completes;
- the final-score screen remains active before its 960-frame limit and completes at the limit;
- a configured play-to value ends immediately at the target score;
- an ordinary result returns to the title scene;
- a tournament result stores the winner, clears the in-progress flag, advances the bracket, and returns to the tournament scene.

`--test-all` includes this suite. The remaining One-on-One rules are tracked by `behavior.one_on_one_rules` and receive no credit from this lifecycle checkpoint.
