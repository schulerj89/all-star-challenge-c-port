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
| Loose-ball pickup | After first ground contact clears the flight lock, height below `$18` plus `$077D`'s strict rectangular proximity limits select the first eligible recovering player; the shot clock resets only when the possession side changes. |
| Pickup cooldown | `$2AE2` decrements `$C12D` before its early exits; `$2B88` requires it to be zero and reloads it with 20 frames when possession is awarded. |
| Pickup global locks | `$2AE2` rejects recovery during `$FFEB` counted waits. `$2B88` then rejects a pending `$FFE2` score event, an `$FFE7` gameplay transition, or `$FFF8` before the first ground contact. The live scene supplies the corresponding native states after its score/lifecycle early returns. |
| Outer dead-ball boundary | `$1CED->$1F4D` stops planar ball motion; it does not directly call a turnover or award possession. |

The post-score ROM branch is at `$0C13..$0C2C`: when `$FF96` is nonzero it reverses `$FFD0` before the possession setup call at `$20F7`. Together with the settings-screen label and manual rule, this is the winners-outs behavior implemented by `allstar_one_on_one_next_possession_after_score`.

Run:

```powershell
.\build\allstar_port.exe --test-one-on-one-lifecycle
```

## Remaining rule gap

`behavior.one_on_one_rules` remains **partial**, not verified. Traveling, first-contact/low-ball recovery, exact `$077D` pickup limits, player priority, the `$C12D` cooldown, all four recovery locks, shooter-action exclusion, side-change shot-clock handling, and the outer dead-ball response are implemented. Collision penalties, steals, blocks, goaltending, shot contests, and full contact outcomes are still absent or generic. The possession coverage must not be read as completion of the full rules milestone.
