# The Dunk, `$70CB` and `$7F0A`

Added **2026-08-27** after a report that dunking did not feel right.

## What a dunk is

There is no separate dunk move. A dunk is the **phase-two shot**: the same
gather, converted mid-air.

```
A            -> $0AA3 gathers; the action becomes $0A or $12, so the actor is
                no longer "eligible" and later updates take $702D's other half
B (fresh)    -> $70CB arms it: player +$13 = 1 and $C16A = 1
next update  -> $70AF decrements $C16A to zero, reaches $70E1, bumps +$13 to
                two and falls through $70F2 into $7C58
$7C58        -> a nonzero phase diverts to $7F0A
$7F0A        -> zeroes $C0A0/$C0A1 and $C0A4/$C0A5 (all planar velocity) and
                sets $C0A8/$C0A9 to $FF00, a raw vertical velocity of -$0100
```

So the ball leaves with **no horizontal motion at all** and drops straight
down, and `$6A8C:$6B34-$6B59` selects display frame `$13` on the next record
load. The animation side of that was already right.

## What was wrong

`$70CB` reads player **+$11**. The player records live at `$FF9D` and `$FFB6`
(`$7008` and `$7010`), so +$11 is `$FFAE`/`$FFC7` — the **new**-input byte —
and +$12 is `$FFAF`/`$FFC8`, the held one. `$70C2` reads the same +$11 byte one
instruction earlier for A.

That `$FFAE` is the edge byte is not a guess: `$02DB` masks it with `$33` to
toggle the title screen's player count, and that toggles once per press rather
than every frame while a direction is held.

The port armed the dunk from `held_input`. The visible consequence:

| | cartridge | port before this fix |
|---|---|---|
| press A, then press B | dunk | dunk |
| **hold B, then press A** | **ordinary jump shot** | **dunk** |

So any shot taken while B happened to be down became a dunk — the ball dropped
vertically instead of arcing at the hoop. That is the "dunking isn't right"
report.

## The test that hid it

An existing case in `--test-one-on-one-shooting` set `new_input = 0`,
`held_input = 2`, and asserted the dunk armed. It encoded the implementation
rather than the cartridge, so it would have passed the bug indefinitely — and
it failed the moment the code was corrected, which is how it was found.

It now uses a fresh press, and `--test-dunk` covers both directions
explicitly: a fresh B arms, held-only B does not.

## Verification

```powershell
.\build\allstar_port.exe --test-dunk
```

The test also checks `$70D2` (no ball, no dunk), that `$70E1` reaches phase two
on the following update and emits the release, that `$7F0A`'s vector is exactly
`(0, 0, -$0100)` and stays planar-free for twenty frames of integration, and
that a phase-zero shot from the same spot still launches with an arc.

Mutation-checked by restoring the `held_input` read, which fails with
`held B armed the dunk without a fresh press`.

`--dump-screenshots` is unchanged at 60 of 60 against the baseline: this is an
input-timing fix, not a rendering one.
