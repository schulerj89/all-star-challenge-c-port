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

## The `$2578` status panel

Added **2026-08-26**. `$2578` is one of **four** mode-indexed dispatchers in the
cartridge, alongside `$10E1`, `$231E` and bank 1 `$7182`. (An earlier revision
of this section called it the third and last; that was wrong, and the count is
corrected in `MENU_AND_AUDIO.md`.) `$147B` and `$10D9` are two further `rst $08`
tables that index on `$C181` and `$FF8D` rather than on the mode.

| mode | `$257B` target |
|---|---|
| 0 One-on-One | `$2585` |
| 1 Free Throw | `$25ED` |
| 2 H-O-R-S-E | `$2607`, a bare `ret` — this mode draws no panel |
| 3 Accuracy | `$2608` |
| 4 Tournament | `$2585`, the same handler as One-on-One |

`$2608` draws two fields of its own and then **jumps into `$2585`'s tail** at
`$25C5` to share the last one, which is why Accuracy places it at `$0D05`
instead of `$0F0C`.

### Fields

Every field is a short run of tiles terminated by a byte with bit 7 set.
`$2517` walks a list to entry `$FF8D`, measures it, and writes it at DE.

| mode | source | index from | how | at |
|---|---|---|---|---|
| 0, 4 | `$FF92` as digits, or `$2559` when it reads zero | — | — | `$0F09` |
| 0, 4 | `$253A` | `$FF97` | minus one | `$0F0A` |
| 0, 4 | `$253D` | `$FF96` | direct | `$0F0B` |
| 1 | `$2543` | `$FF98` | `$05`→0, `$10`→1, else 2 | `$0904` |
| 3 | `$253D` | `$FF9B` | direct | `$0F03` |
| 3 | `$253D` | `$FF9A` | direct | `$0F04` |
| 0, 3, 4 | `$2551` | `$FF95` | `$02`→0, `$05`→1, `$08`→2, else 3 | `$0F0C`, or `$0D05` in mode 3 |

### The digit path

`$24E4` splits the `$FF92` word into four nibbles, high to low, and `$2500`
looks each up in the table at `$250D`, writing the results to `$C1A1`. That
table holds `$01`..`$0A`, so the lookup is just **digit plus one** — a different
base from the `$C1` the score fields use in `$1726` and `$780A`, and with no
leading-zero blanking.

Run:

```powershell
.\build\allstar_port.exe --test-status-panel
```


## The `$231E` settings-screen cursor

The fourth mode dispatcher. Its table at `$2321` gives each mode its own cursor
table, row count and row-handler list:

| mode | handler | cursor table | rows | wrap |
|---|---|---|---|---|
| 0 One-on-One | `$232B` | `$2AC8` | 4 | `and $03` |
| 1 Free Throw | `$235F` | `$2AD0` | 1 | none |
| 2 H-O-R-S-E | `$236A` | `$2AD2` | 1 | none |
| 3 Accuracy | `$2374` | `$2AD4` | 3 | explicit compares |
| 4 Tournament | `$2330` | `$2ADA` | 4 | `and $03` |

`$232B` and `$2330` differ only in which table they load and then share the body
at `$2333`; `$2AC8` and `$2ADA` happen to hold the same four `(y,x)` pairs. Up
(`$FFAE` bit 6) is tested before Down (bit 7), so pressing both moves up.

The two four-row modes wrap with `and $03`, while the three-row mode compares
against `$03` going forward and `$FF` going back — two different wrap idioms in
handlers that are otherwise identical.

Run:

```powershell
.uildllstar_port.exe --test-shell
```
