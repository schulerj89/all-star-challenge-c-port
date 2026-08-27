# Boot and Title, `$0150` and `$029C`

Added **2026-08-26**. Ported from `$0150..$0212` and `$029C..$030D`.

## Two entries, one wipe

`$0150` is the cold entry from `$0101`. `$0156` is the soft-reset vector the
watchdog at `$2D1B` jumps to. Both converge on `$0170`, which disables
interrupts, resets the stack to `$E000` and wipes three regions:

| region | bytes |
|---|---|
| `$C000` work RAM | `$02D0` |
| `$DD72` sound state | `$00BD` |
| `$FF80` HRAM | `$007E` |

## The seed survives the wipe

The RNG seed at `$FFFB..$FFFE` sits **inside** the HRAM region, so `$01A8` and
`$01AF` push both words onto the stack before the wipe and `$01C4`/`$01CB` pop
them back after. `$C191` rides through the work-RAM wipe in `$FF8C` the same way.

Without that, every soft reset would start from the same seed and replay the
same game. The test asserts the preserved addresses actually fall inside the
wiped range, so the point of the push cannot be lost to a later edit.

## A link partner is told about the reset

`$0156` checks two things before wiping: this must be a link game (`$C18B`) and
this cartridge must hold role `$02`. Only then does it post `$C3` into `$C18E`
and `$C270` and wait five vblanks for it to go out.

`$C18E` is the same mailbox the pause handler at `$2BF9` posts `$CC` into. That
makes two protocol bytes on that channel and, with the title handshake below,
brings the link sites to eight — see `LINK_CABLE.md`.

## The outer loop, `$01F1`

`$01E3` runs once, then `$01F1` loops forever: page in bank 3, run the title at
`$029C`, and either fall into attract when `$FFE4` is set or run the menu and a
game before looping. `$C191` is set to `$01` on the way in, which is the flag
`$0150` preserves — so the game can tell a soft reset from a cold start.

## The title selector, `$02AC`

`$FF91` holds the player count and toggles between one and two by
`dec / xor $01 / inc` — **not** a plain `xor`, which would produce zero. The
input masks use the cartridge packing: `$33` (A, B, Right or Left) toggles and
`$08` (Start) confirms.

A `$0960` frame counter runs down the whole time. Only the idle path spends it;
a toggle returns without decrementing. When it reaches zero `$FFE4` is set and
the game drops into attract mode.

Confirming with two players does not return — `$0322` takes `$C199 = $01` and
enters a ten-attempt link handshake. That is where the role byte the whole
serial layer depends on is first assigned.

Run:

```powershell
.\build\allstar_port.exe --test-boot
```

## The handshake, `$0324` and `$035F`

Two entries, and which one a cartridge takes decides its role for the rest of
the session.

**`$0324` initiates.** Ten attempts, each claiming role `$01` afresh and then
taking three readings of `$C19E`, one per vblank:

| reading | on success | on failure |
|---|---|---|
| first, `== $01` | skip the middle exchange | echo `$01` and take the middle reading |
| middle, `== $01` | continue | **abort immediately**, without spending an attempt |
| last, `== $02` | become role `$02` | spend an attempt and retry |

The middle failure aborting *without* spending a try is the part worth noticing:
only the last reading costs an attempt, so ten retries means ten failures at the
final step, not ten anywhere.

**`$035F` answers.** It discards the title loop's saved counter with a bare
`pop bc`, spins until the serial interrupt raises `$C19C`, takes role `$03`,
sends a zero and joins the same tail.

So the side that presses Start first ends at role `$02` and the side that
answers at role `$03` — and `$03` is exactly the value `$267F` keys on to decide
that the byte it receives belongs in pad 1. Both paths finish at `$0376`, which
sets `$FFA5`, `$FFBE` and two players.

## Attract, `$0417`

Sets mode `$00`, clears `$C19A`, loads `$0E10` into `$C26D`, seeds `$FF94` with
`$0200`, picks skill `$01`, and calls the bank 2 selector.

`$0E10` is sixty seconds of frames, and it is **the same counter `$2D1B`
decrements** in attract mode — the watchdog ported earlier spends exactly what
this sets. The selector call lands on `$4003`'s `$FFE4` check, which diverts to
the `$2D85` CPU auto-pick, so attract drives the entrant screen by itself. Skill
`$01` is the easiest, which `$761B` reads as index zero.

The test asserts both of those couplings rather than just the constants, so the
attract countdown and the watchdog cannot drift apart.

## Scope

This ports the wipe layout, the preserved words, the reset notification, the
title toggle, the confirm branch, both handshake entries and the attract setup.
The bank 2 selector's own attract behaviour was ported earlier as `$2D85`.
