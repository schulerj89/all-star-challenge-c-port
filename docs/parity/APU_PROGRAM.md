# The APU Channel Programmer, `$35B6`

Added **2026-08-26**. Ported from `$35B6..$3714` and its tables at `$3151`,
`$3159`, `$3777`, `$388A` and `$3FB2`.

This is the routine that actually writes the Game Boy sound registers. It takes
a voice slot in BC, reads that slot's kind from `$DD9F`, arbitrates for the
hardware channel, and programs one of the four channels from a byte block.

## Channel arbitration, `$DD7D`

The interesting part. `$3151` gives each kind a single bit — `$01 $02 $04 $08
$10 $20 $40 $80` — and the slot number decides what happens with it:

| slot | behaviour |
|---|---|
| below `$04` | tests the bit and **gives up** if it is already set |
| `$04` and above | **sets** the bit and proceeds |

So the upper slots take a channel and the lower ones stand down. That is the
whole sound-effect-over-music priority scheme, in eleven instructions. A high
slot is never blocked, because it never tests before setting.

## The four branches

`$DD9F` names the kind. Only `$00`, `$01` and `$02` are matched; everything
else falls through to noise.

| kind | branch | NR51 keeps | pan table | block |
|---|---|---|---|---|
| `$00` square 1 | `$35F0` | `$EE` | `$3777` | `$388A` |
| `$01` square 2 | `$3631` | `$DD` | `$377B` | `$388B` |
| `$02` wave | `$366F` | `$BB` | `$377F` | `$388A` |
| else noise | `$36DB` | `$77` | `$3783` | `$388B` |

Square 2 and noise read their block **one byte later** than the other two,
because neither has a sweep register to load.

## Panning

Bits 3 and 2 of the first block byte index a four-entry table: silent, right,
left, both. Each channel's table holds its own bit in each nibble, so
`$3777` is `$00 $01 $10 $11` and `$3783` is `$00 $08 $80 $88` — the standard
NR51 layout, low nibble right and high nibble left.

## The registers

| kind | writes |
|---|---|
| square 1 | `$FF10` `$FF11` `$FF12`, skip, `$FF13` `$FF14` |
| square 2 | `$FF16` `$FF17`, skip, `$FF18` `$FF19` |
| wave | `$FF1A` (masked `$80`) `$FF1B` `$FF1C`, skip, `$FF1D` `$FF1E` |
| noise | `$FF20` `$FF21` `$FF22` `$FF23` |

The skipped byte is a bare `inc hl` in each branch. The frequency low byte comes
from `$31C6` indexed by the note in `$DDEF`, and the high byte is the block byte
masked with `$F8` ORed with `$3159`'s entry for that note.

**Every byte of `$3159` in this ROM is zero**, so that OR never changes
anything. The lookup is reproduced anyway, because the table is real and a
different build could fill it.

## Waveform upload

`$366F` is the one branch that touches wave RAM. The low nibble of its block
byte is a waveform id, compared against the cache in `$DD78`. **Only on a
change** are sixteen bytes copied from `$3FB2 + (id << 4)` into `$FF30`. The
shift is four `rlca` instructions, so the stride is sixteen bytes per waveform.

Run:

```powershell
.\build\allstar_port.exe --test-apu-program
```

## Scope

This ports the arbitration, the dispatch, the panning arithmetic, the register
order and the waveform cache. The block data at `$388A` and the waveform bank at
`$3FB2` are ROM data the asset pipeline would supply; the routine's callers,
`$34FA` and the `$3014` interpreter, are separate work.
