# Settings Persistence Evidence

Last verified: **2026-08-14**

## Scope

This checkpoint covers the ROM's configurable values, their session lifetime, their menu cycles, and downstream native consumers. The cartridge has no RAM, so "persistence" means settings survive scene changes for the running session; starting a new process resets them to the ROM defaults. Mode-level parity is documented separately for Free Throw and Accuracy.

## ROM behavior

`$20D0` initializes the settings HRAM fields, `$22EF` edits and draws the mode-specific values, and `$1FFA` converts skill level to a gameplay update delay.

| Setting | ROM storage/default | Recovered choices | Native consumer |
|---|---|---|---|
| Play to | `$FF92/$FF93 = 0` | Time, or `1..99` points | One-on-One `play_to` ending |
| Skill level | `$FF97 = 1` | `1`, `2`, `3` | CPU decision delays of `8`, `4`, or `1` frames |
| Winners outs | `$FF96 = 0` | No/Yes | Post-score possession selection |
| Time limit | `$FF95 = $02` | `2`, `5`, `8`, `12` minutes | One-on-One and Accuracy clocks |
| Free Throw attempts | `$FF98 = $05` | BCD `$05`, `$10`, `$20` (5, 10, 20 attempts) | Free Throw attempt limit and HUD |
| Accuracy positions | `$FF9A = 1`, `$FF9B = 0` | Computer positions or new positions | Accuracy position layout source |

The Accuracy values are one binary choice, not two independent flags. `$24C4` toggles `$FF9A` and `$FF9B` together, keeping them complementary. The decoded mode-3 settings screen labels the rows `New pos.` and `Computer pos.`; bank 1 `$6CA2` takes the computer-generated-position path when `$FF9A` is set.

## Native design

`AllStarGameSettings` is owned by `AllStarGame`, initialized once, and no longer recreated by `scene_settings.c`. Revisiting Settings therefore preserves earlier choices. One-on-One consumes play-to, time, winners-outs, and skill; Free Throw consumes its attempt count; and the current Accuracy scene consumes time and the position-source choice.

Accuracy consumes the choice directly: computer positions use
`$6CA2/$6CAB`'s five groups of ten ROM pairs, while new positions enter
`$6D57`'s four-pixel marker editor and record ten pairs before play. The
native regression checks the two controller states directly.

## Deterministic verification

Run:

```powershell
.\build\allstar_port.exe --test-settings
```

The suite checks ROM defaults, exact time/attempt cycles, the `8/4/1`-frame skill table, settings-scene round trips, complementary Accuracy selection, distinct native Accuracy layouts, Free Throw attempt-limit presentation, and One-on-One consumption of play-to, winners-outs, and time.

`--test-all` includes this suite.
