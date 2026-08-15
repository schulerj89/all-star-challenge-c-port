# One-on-One Possession Evidence

Last reviewed: **2026-08-14**

## Implemented scope

The native One-on-One rules now centralize possession changes and shot-clock resets. The following paths are covered by deterministic tests:

| Event | Native result |
|---|---|
| Shot-clock expiration | Ball changes sides and the shot clock resets. |
| Made basket, winners-outs off | The non-scoring player receives the next possession. |
| Made basket, winners-outs on | The scorer retains the next possession. |
| Defensive rebound | Possession changes and the shot clock resets. |
| Offensive rebound | Possession remains with the offense and the current shot clock is preserved. |

The post-score ROM branch is at `$0C13..$0C2C`: when `$FF96` is nonzero it reverses `$FFD0` before the possession setup call at `$20F7`. Together with the settings-screen label and manual rule, this is the winners-outs behavior implemented by `allstar_one_on_one_next_possession_after_score`.

Run:

```powershell
.\build\allstar_port.exe --test-one-on-one-lifecycle
```

## Remaining rule gap

`behavior.one_on_one_rules` remains **partial**, not verified. Collision penalties, steals, blocks, goaltending, traveling, shot contests, and ROM-matched rebound/contact outcomes are still absent or generic. The new possession coverage must not be read as completion of the full rules milestone.
