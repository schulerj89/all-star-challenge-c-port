# The Frame Spine, `$2729` / `$276D` / `$279E`

Added **2026-08-27**. Ported from `$2729..$27C6`, plus the lifecycle routines
around it: `$0271`, `$1F7A`, `$1FA4`, `$1FE1`, and the banner at `$1699`.

These seven were the largest remaining group of reachable, unported ROM code.
They turn out to be one story rather than seven separate routines.

## The handler, `$2729`

```
di / push af
  role $03 and $C176 != 0  -> inc $C176, and nothing else at all
  role $03 and $C176 == 0  -> $FF80 (OAM DMA), but no frame update
  any other role           -> $FF80, then $2757
wait for STAT bit 1 to rise, then to fall
pop af / reti
```

The stalled branch is worth spelling out: a role `$03` cartridge with a nonzero
`$C176` **does not even copy OAM**. It spends the whole interrupt incrementing
that counter. `$1FD4` clears `$C176` on the way back to the title, which is
what releases it.

The tail waits for `$FF41 & $02` to go high and then low. Bit 1 of STAT is set
in modes 2 and 3, so the handler returns at a fixed point in the scanline
rather than wherever the frame work happened to finish.

## The body, `$276D`

`$2757` sets `$C140`, saves the current bank in `$C13D`, pages in bank 1, calls
`$276D`, and pages back. `$276D` then:

- burns 256 `push af` / `pop af` pairs when `$C19D` is set — the serial settle;
- runs `$2624` (the pad poll, which is `$2639`) and `$279E` (the link send)
  **in an order decided by the role**: role `$03` sends first, everyone else
  polls first, so the two cartridges are not transmitting from the same half of
  the frame;
- increments `$FF8B`;
- decrements `$FF8A` if it is nonzero, stopping at zero rather than wrapping.

`$FF8B` is not just a tick. The postgame banner is called as
`ldh a,[$ff8b] / and $08 / call $1699`, so this counter is what makes the
winner line flash — sixteen frames on, sixteen off. The test asserts that by
running `$276D` thirty-two times and counting.

## The send, `$279E`

Modes `$01` and `$03`, with two players and `$FF90` set, push `$C16E`, put
`$C18D` there for the single `$2FD0` call, and pop the original back. Every
other case falls straight through to `$2FD0` with `$C16E` untouched.

`$C16E` is the byte `$2639` writes and `$2FD0` transmits, so this is a
deliberate substitution on the wire for one frame. The test proves the coupling
by feeding `$279E`'s staged byte into the already-ported `allstar_link_transmit`
rather than comparing constants.

## The copyright screen, `$0271`

It loads `$640F` from bank 1 into `$9000` and `$4000` from bank 3 into `$9800`,
sets LCDC to `$81`, and holds for `$00F0` — **240 frames, almost exactly four
seconds** — before returning to the title.

It is gated on `$C191`, the warm-boot flag. Since `$0263` sends every finished
game back through `$0156`, which sets `$C191`, the copyright screen appears on
a **cold boot only**. The second and later games of a session go straight to
the title.

The port's intro scene had been showing it for 2.5 s on every entry.

## Leaving a game, `$1F7A` / `$1FA4` / `$1FE1`

- **`$1F7A`** clears `$DD72` and `$DD73` — both sound mailboxes — which is what
  actually stops the music, then loads two tables. `$029C` re-posts `$81` into
  `$DD73` when the title comes back up; see `TITLE_MUSIC.md`.
- **`$1FA4`** masks LCDC down to bit 7, sets the player count back to one, wipes
  the 160-byte `$C000` OAM shadow through `$048B`, and clears twelve bytes.
  Two of them tie back to routines already ported: `$C16E` (`$2639`'s outgoing
  pad byte) and `$C176` (`$2729`'s stall counter above).
- **`$1FE1`** clears SC and SB, the role `$C199`, and the three handshake bytes,
  seeds `$C1A0` with `$B3`, and tails into `$2FE3`.

The three clear-lists are asserted in ROM order, and the test also checks that
no list names the same address twice.

## The banner, `$1699`

Three outcomes, chosen from `$FF8F` and the winner:

| condition | line |
|---|---|
| the caller's `$FF8B & $08` is zero | fifteen spaces (`$16FD`) |
| winner is zero, outside H-O-R-S-E | `A TIE` (`$16F7`) |
| winner is zero, in H-O-R-S-E | still blank — the game is not over |
| winner is 1 or 2 | `<name> WINS`, name from `$C23C` or `$C255` |

H-O-R-S-E takes its winner from `$C17D` rather than `$28E1`, trims leading
spaces through `$1769`, and places the line at `$020A` instead of `$0302`.

## Verification

```powershell
.\build\allstar_port.exe --test-frame
```

Mutation-checked twice: letting a stalled role `$03` copy OAM fails with
`$2739 stall path diverged (dma=1 ...)`, and showing the credits on a warm boot
fails with `$0275 showed the credits on a warm boot`.

## Where this sat in the sweep

This chunk took reachable ROM code not named in `src/` or `include/` from
1,287 bytes to 828. The sweep finished later the same day; see
`docs/GHIDRA_COVERAGE.md` for the final figure rather than trusting the
running total quoted here.
