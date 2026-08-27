# The Defensive Jump, `$6BF9` / `$6C27` / `$6C4D`

Added **2026-08-26** after a report that jumping on defense froze the game.

## The symptom

Pressing A without the ball produced a defender who stood perfectly still for
72 frames — 1.2 seconds — and could not be moved. That is a long time to be
unable to do anything, and with nothing at all happening on screen it read as
a lock-up rather than as an animation.

The port was not hung. Every routine involved was already correct about
*state*: `$702D` took the `$70FD` branch, the action became `$05`/`$0C`/`$14`,
the twelve six-frame records ran, the twelfth record's `$02` control byte
returned the actor to a movable action, and the defender recovered. What was
missing was the only part of a jump you can actually see.

## What the ROM does

`$6A8C` calls `$0A78` at `$6B68` and, for the eight protected actions, jumps
to `$6BF9` instead of the normal movement path. `$6BF9` falls through to
`$6C27`:

```
$6C27  ld hl,$0003 / add hl,de / ld c,[hl]     ; c = player +$03, the record
$6C2E  ld hl,$6c4d / add hl,bc / ld a,[hl]     ; a = the record's delta
$6C33  ld hl,$0005 / add hl,de / add [hl] / ld [hl],a
```

`$6C4D` is twelve signed bytes:

```
00 F7 F9 FB FD FE 00 02 05 07 08 04
 0  -9  -7  -5  -3  -2   0  +2  +5  +7  +8  +4
```

They accumulate into player **`+$05`**, and `$2945` writes `+$05` straight to
OAM Y. So the sprite rises and falls along the running sum:

```
record   0   1   2   3   4   5   6   7   8   9  10  11
lift     0   9  16  21  24  26  26  24  19  12   4   0
```

The ground anchor **`+$15`** does not move. `$6BA3` only refreshes it as
`+$05 + $28` on the grounded path, and `$6C27`'s two exits skip that. Two
things follow, and both were already right in the port:

- `$2B6C` reads reach as `+$15 - +$05`, which is `$28` on the floor and grows
  to `$28 + 26` at the top of the jump.
- `$077D`'s planar gate reads `+$15`, so an airborne defender still overlaps
  the ball from where their feet were.

## What was wrong

`scene_one_on_one.c` computed the sprite's `visual_lift` **only** from
`p1_shot_animation_clock` — the shot gather. A defensive jump has no shot
clock, so its lift was always zero and the sprite never left the floor. The
`$6C4D` table existed in the port, but only in its cumulative form and only as
the reach input to `$2B6C`; nothing ever put it on screen.

`allstar_one_on_one_rom_jump_lift_delta_6c4d` now holds the twelve raw signed
bytes, `allstar_one_on_one_rom_jump_lift_6c27` sums them, and
`allstar_one_on_one_rom_jump_height_6c4d` — the `$2B6C` reach — is derived
from the same sum rather than from a second hand-copied table.

The lift is resolved inside the animation step, where `$6C27` itself runs, and
one detail matters there: `$6C47` increments `+$03` *after* `$6C27` has applied
that record's delta, so a stored record index is one past the record on screen.
`one_on_one_displayed_record_6c47` does that conversion.

## What is deliberately unchanged

Standing still for the full 72 frames is correct. `$702D` bypasses its
direction refresh for the protected actions, so a jump only travels if a
direction was already latched in `+$07` when it started — press A alone and the
ROM's defender also lands exactly where they took off. The port matched this
before and still does.

## Verification

```powershell
.\build\allstar_port.exe --test-defense-jump
```

The test asserts the twelve `$6C4D` bytes individually, that the lift is their
running sum, that `$6BF9`'s gating admits only `$05`/`$0C`/`$14`, and that
`$2B6C`'s reach agrees with the same sum. It then drives the scene through a
real jump and requires the sprite to reach the `$6C4D` peak of 26, come back
down, land at zero, and leave the `+$15` anchor on the floor the whole time.

Mutation-checked twice: zeroing the lift (the behaviour that was reported)
fails with `defensive jump peaked at 0`, and changing one `$6C4D` byte fails
with `$6C4D+11 is 5, expected 4`.
