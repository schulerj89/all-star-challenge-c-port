# Rim and Backboard Outcome Evidence

Added **2026-08-26**. Covers the `$1AF9` dispatch table, its handlers at
`$1B3F..$1BBC`, and the bounce-and-score routine at `$1E74`.

## Why this is shared engine

One-on-One, Free Throw and H-O-R-S-E all reach this table, so a divergence here
shows up in three modes at once. `$1E74` is also where points are added, which
makes it the most load-bearing routine outside the match driver.

An independent trace of the One-on-One closure from `$0B80` found **no
One-on-One-specific gaps at all**: all 29 unimplemented routines on that path
are shared with at least one other mode. This group was the largest of them.

## The `$1AF9` table

Eleven slots, indexed by C at `$1AF7`. Slots 2 and 8 both land on `$1E74`.

```
$1BA7  $1B3F  $1E74  $1B93  $1B53  $1BBD  $1B59  $1B99  $1E74  $1B45  $1BAD
```

`$1BBD` was already ported; the other nine are new here.

## The handlers

Each is the same three instructions — store a signed 16-bit horizontal velocity
into `$C0A0/$C0A1` — followed by one of three exits.

| entry | velocity | exit |
|---|---|---|
| `$1B3F` | −50 | `$1E74` |
| `$1B45` | +50 | `$1E74` |
| `$1B93` | +110 | `$1E74` |
| `$1B99` | −110 | `$1E74` |
| `$1BA7` | −110 | `$1C12` then `$1C0F`, skipping `$1E74` |
| `$1BAD` | +110 | `$1C12` then `$1C0F`, skipping `$1E74` |
| `$1B53` | +165 | the settle path at `$1B5D` |
| `$1B59` | −147 | the settle path at `$1B5D` |

The values come in mirrored pairs except the settle pair, which is
**deliberately asymmetric**: +165 against −147.

### The settle path, `$1B64..$1B90`

The ROM branches on B — the velocity's *high byte* — so the test is on the sign,
not the magnitude. A rightward bounce resets the vertical velocity to `$0080`
and clears `$C0A4`, `$C0A5` and `$C0A9`. A leftward bounce counts up in `$C164`,
and from the **second** one on it drops the height in `$C0A3` by three and exits
through `$1C05` instead.

## `$1E74`, the bounce and the score

1. `$1E7A..$1E8C` reverses the vertical velocity at `$C0A8` by two's complement.
2. `$1E8F..$1EBC` adds a damping value:

   | condition | damping |
   |---|---|
   | mode `$01` and `$C0AB` clear | −250 (also sets `$C128 = $04`) |
   | mode `$01` and `$C0AB` set | −57 (also sets `$C128 = $04`) |
   | other modes, `$C0AB` set | −57 |
   | other modes, `$C0AB` clear, `$FFD4` clear | −57 |
   | other modes, `$C0AB` clear, `$FFD4` set | −300, and `$FFD4` is consumed |

3. `$1ECC..$1EDA` runs a fifteen-frame timer in `$C168`. Each expiry reloads 15
   and consumes one step from `$C129`, redrawing a 3×2 tile block at `$9849`.
4. `$1EF4..$1F29` reacts to the step count. The rim cue (sound `$08`) fires on
   the step where `$C129` equals the threshold — three, or two when `$C12A` is
   set. The score lands only at zero: sound `$05`, then `$290B` adds **two
   points, or three when `$FFD7` is set**, to `$C133` or `$C135` depending on
   whether `$C17A` is `$02`.

Run:

```powershell
.\build\allstar_port.exe --test-shot-result
```
