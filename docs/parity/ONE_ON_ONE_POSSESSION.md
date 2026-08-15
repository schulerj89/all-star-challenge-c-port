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
| Traveling after an unreleased jump | The defender receives possession and the shot clock resets. |
| Loose-ball pickup | `$077D`'s strict rectangular proximity limits select the recovering player; the shot clock resets only when the possession side changes. |
| Outer dead-ball boundary | `$1CED->$1F4D` stops planar ball motion; it does not directly call a turnover or award possession. |

The post-score ROM branch is at `$0C13..$0C2C`: when `$FF96` is nonzero it reverses `$FFD0` before the possession setup call at `$20F7`. Together with the settings-screen label and manual rule, this is the winners-outs behavior implemented by `allstar_one_on_one_next_possession_after_score`.

Run:

```powershell
.\build\allstar_port.exe --test-one-on-one-lifecycle
```

## Remaining rule gap

`behavior.one_on_one_rules` remains **partial**, not verified. Traveling, exact `$077D` pickup limits, side-change shot-clock handling, and the outer dead-ball response are implemented, but `$2B88` cooldown/flight gates, collision penalties, steals, blocks, goaltending, shot contests, exact control timing, and full rebound/contact outcomes are still absent or generic. The possession coverage must not be read as completion of the full rules milestone.
