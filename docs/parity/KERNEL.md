# The Kernel, `$0000..$005F` and its helpers

Added **2026-08-27**. This chunk closed the last of the reachable ROM code:
the vector table and seventeen small routines hanging off it.

## The vectors

| address | what it is |
|---|---|
| `$0008` | pops the return address, runs the `$0010` lookup on it, jumps to the entry |
| `$0010` | doubles the index, adds it to the table, leaves the entry in HL |
| `$0020` | the MBC1 trampoline: `$2150` and the `$C13B` shadow |
| `$0028` | save the bank in `$C13C`, wait a vblank |
| `$0030` | bank 1, then `$0B4F` |
| `$0038` | `$FF` filler — an unused trap |
| `$0040` | vblank → `$2729`, the frame spine |
| `$0048` | STAT — a bare `reti` |
| `$0050` | timer — a bare `reti` |
| `$0058` | serial → `$0061` |

`$0008` and `$0010` are the mechanism every ported routine's comments have been
pointing at when they say "`rst $10` picks between them". `$0008` is the
jump-to form and `$0010` the leave-it-in-HL form; both read the inline table
that follows the call site.

## Helpers worth recording

**`$045E`** clears the pending-interrupt flags in `$FF0F` **before** writing the
new mask to `$FFFF`, so enabling a source cannot immediately fire on a stale
flag.

**`$0466`** copies ten bytes from `$0474` into `$FF80`: the canonical OAM DMA
routine, which has to run from HRAM because the bus is otherwise unavailable
during the transfer. Those ten bytes are what `$2729` calls every frame.

**`$0A91`** clears the frame counter and the whole RNG state — `$FF8B`,
`$FFFB`–`$FFFE`, `$FFF9` — but **only in a one-player game**. `dec a` / `ret z`
means a link game returns untouched, so both cartridges keep the seed they
agreed on. That is the counterpart to `$0150` preserving the seed across the
work-RAM wipe.

**`$2F79`** is a two-stage countdown: `$C194` runs down on every call, and only
the frame it reaches zero clears `$C193`.

**`$27EA`** waits `$C195` vblanks, reloads it with `$0B`, and dispatches through
`$C197` using `$C196` as the step — which it **post**-increments, so the step
that runs is the value from before the bump.

**`$3119`** resets the APU: NR51 to zero, the wave cache to `$FF`, NR52 to
`$8F`, and **NR50 to `$77`**. That last one matters beyond this routine: master
volume is maximum and symmetric, which is why the title song's NR51 routing is
the only thing placing its voices. See `TITLE_MUSIC.md`.

**`$7015`** parks the entity id in `$C187` and then, in H-O-R-S-E and Accuracy
only, returns unless this entity is the one `$FFDA` names. The single-shooter
modes freeze the other player's animation rather than hiding it.

**`$7AEA`** has two gates — one player, and the shooter must not be one — and
then clears the same `$C0FD`/`$C145` pair `$0D2B` clears, which is what makes
those two a matched pair around a turn.

**`$7712`** writes two HUD values whose readers differ: `$772D` dereferences a
word through `$C133`, while `$7721` takes the single byte at `$FFAB` and
zero-extends it.

## Verification

```powershell
.\build\allstar_port.exe --test-kernel
```

Mutation-checked twice: making `$2F79` clear `$C193` every frame fails with
`$2F82 cleared $C193 early`, and clearing the RNG in link games too fails with
`$0A94 cleared the RNG in the wrong game`.
