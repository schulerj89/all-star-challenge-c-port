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

## Court-wide state

Added the same day: `$1C1D`/`$1C32`/`$1C3E`, `$1F3E`/`$1F5B`, `$2BC6`, `$2C72`
and `$2C95` — 280 more bytes on the same shared path.

### Sprite priority against the backboard, `$1C1D..$1C60`

Four sprite groups at `$C000`, `$C010`, `$C020` and `$C030`. For each, `$1C3E`
compares the group's Y against `$58`: at or above it the four attribute bytes
get bit 7 **set** (drawn behind the background), below it the bit is cleared.
`$1C32` then overrides every group to "behind" whenever `$C12B` says the ball is
held. The comparison is `cp $58 / jr nc`, so the boundary is inclusive.

### The rim-cue selector, `$1F3E`

Either `$FFB0` or `$FFC9` being set writes `$06` into `$C12A`, which is what
makes `$1E74`'s rim cue fire at two steps instead of three. That closes the loop
with the shot-result group above.

`$1C05` is the other half of that loop: the settle path's second leftward bounce
sets `$FFD4`, which is exactly the flag `$1E74` consumes for its −300 damping.

### The pause toggle, `$2BC6..$2C43`

Start (`$FFAE` bit 3) is gated on **six** flags all being clear: `$C185`,
`$C16F`, `$C12E`, `$FFEB`, `$C174` and `$FFEC`.

In a link game (`$C18B`) the two cartridges behave differently. The side that is
not player 2 posts `$CC` into `$C18E` — a pause request sent over the link — and
returns without pausing, dropping the request if one is already pending. Player 2
clears `$FFAE` to consume the button and drives the toggle itself. `$FFE5` is the
paused flag; entering plays sound `$01` and posts `$069E`, and mode `$01` hides
and restores OBJ around it.

This is the fifth place the link cable appears in the ROM, after `$267F`,
`$1121`, `$1209` and the `$C199` role byte.

### The two message triggers

`$2C72` posts `$068D` when a 16-bit counter reaches zero, recording the owner in
`$FFD0` and setting `$C131`.

`$2C95` posts `$0649` when a player's action is `$03`, `$0A` or `$12`, its
sub-state is `$0C`, and `$FFCF` says that player holds possession. It discards a
return address before tail-jumping into `$05A3`, so the message writer returns
past its own caller — modelled explicitly as `unwinds_caller`.

Run:

```powershell
.\build\allstar_port.exe --test-court-state
```

## The court clocks, `$79EE` and `$7A71`

Three clocks, each a little-endian pair with **BCD seconds in the low byte and
BCD minutes in the high byte**, each gated by one bit of `$C0BD`:

| clock | enable bit |
|---|---|
| `$C0B6/$C0B7` game | 0 |
| `$C0B8/$C0B9` player 1 | 1 |
| `$C0BA/$C0BB` player 2 | 2 |

`$C0BC` counts twenty calls between ticks. `$FFEB` or `$C12E` stops everything.

Possession in `$FFCF` decides which player clock runs. The player *without* the
ball has theirs reset to `$24` — twenty-four seconds, the shot clock — and its
enable bit cleared, so only one of the two ever advances. Accuracy (`$FF8F ==
$03`) or a loose ball resets both. Bit 0 is preserved through all of it, because
the game clock is enabled elsewhere.

`$7A71` is the shared step: seconds down by one in BCD, wrapping to `$59` with a
minute borrow, and a clock already reading `00:00` is left alone. Both `daa`
sites are reached with carry clear and with a non-zero operand, so the only
adjustment that ever fires is the half-carry subtract of six.

The warning at `$7A38` plays sound `$0B` while the game clock shows no minutes
and its seconds read below `$12` — but not at exactly 1.

### A latent ROM bug

`$7A3A` and `$7A3E` test for modes `$01` and `$02` and branch to `$7A5A`, which
is a `pop hl` with **no matching `push`**. On the cartridge that would take the
return address into HL and then return through whatever was underneath it.

It is unreachable in practice: bit 0 of `$C0BD` is never set in Free Throw or
H-O-R-S-E, so `$7A34` always branches away before those tests run. The port
reproduces the branch — no warning for those modes — without the stack damage,
which is identical behaviour everywhere the ROM can actually reach.

Run:

```powershell
.\build\allstar_port.exe --test-game-clock
```
