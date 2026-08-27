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

## Scope

This ports the wipe layout, the preserved words, the reset notification, the
title toggle and the confirm branch. The handshake loop itself at `$0324`, and
the attract path at `$0417`, are separate work.
