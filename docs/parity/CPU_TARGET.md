# CPU Steering, `$7182` and `$7190`

Added **2026-08-26**.

`$7182` is the fourth mode-indexed dispatcher in the cartridge, alongside
`$10E1`, `$231E` and `$2578`:

| mode | target |
|---|---|
| 0 One-on-One | `$7190` |
| 1 Free Throw | `$718F`, a bare `ret` |
| 2 H-O-R-S-E | `$74A8` |
| 3 Accuracy | `$718F` |
| 4 Tournament | `$7190` |

Everything under `$7190` converges on one thing: choosing a target position and
writing it to `$C101`/`$C102`. That is the same pair `$7367` fills from the
fourteen-pair coordinate tables at `$7391` and `$73AD` — the tables an earlier
pass had mistaken for routines. Three separate paths write that pair, and this
is what they are steering.

## Difficulty, `$761B`

`$761B` indexes a three-byte table by the skill level in `$FF97` minus one, so
every threshold in this routine is difficulty-scaled:

| table | skill 1 | 2 | 3 |
|---|---|---|---|
| `$7626` | `$1B` | `$10` | `$07` |
| `$7629` | `$19` | `$50` | `$96` |
| `$762C` | `$04` | `$19` | `$46` |
| `$7635` | `$1E` | `$14` | `$04` |

**They do not all slope the same way.** `$7626` and `$7635` shrink as skill
rises while `$7629` and `$762C` grow, so "higher skill" tightens some gates and
loosens others depending on which side of the compare the threshold sits.

## Entry, `$7190`

`$FFCF` is compared against `$C127`. Equal means possession has not changed and
control goes to `$72BF`; otherwise six bytes are cleared, in this order:

```
$C0F7  $C0FA  $C0F9  $C0F8  $C100  $C106
```

## Choosing a target

Three different routines compute the pair.

**`$7237`, direction stepping.** The entity's record fields at `+$06` and `+$15`
are offset by `+8` and `+4`, then the direction byte at `+$10` is tested bit by
bit — **the first set bit wins**:

| bit | effect |
|---|---|
| 0 | coarse axis `+$10` |
| 1 | coarse axis `-$10`, **clamped at zero** |
| 2 | fine axis `-8` |
| none | fine axis `+8` |

The clamp is real: `$7272` takes `jr nc` and otherwise `xor a`, so the value
floors at zero rather than wrapping to `$F8`.

Those are the same two record fields, and the same step sizes, that the `$0ADB`
probes move — sixteens on `+$06` and eights on `+$15`.

**`$728C`, court centering.** Pulls the coarse axis toward the middle:

| coarse | change |
|---|---|
| `<= $3C` | `+$10` |
| `> $6C` | `-8` |
| between | `+8` |

The fine axis always loses eight. Both boundaries are inclusive on the low side.

**`$72EA`, spot lookup.** The ball height at `$C0A3` picks between two
four-entry tables at `$731C` (below `$54`) and `$7324` (at or above), and a
value in `$FFFC` buckets into one of four spots at `$30` then steps of `$40`.
The two tables share their coarse column — `$68 $8C $7C $98` — and differ only
in the fine one, so the ball's height changes where on the court the CPU aims
without changing which lane it picks.

Run:

```powershell
.\build\allstar_port.exe --test-cpu-target
```

## The exits, `$73C9..$74A7`

`$7190`'s three jump targets all live in the block that follows the coordinate
tables at `$73AD`. They are what finally acts on the target.

**`$749E`** loads `$C102`/`$C101` and hands them to `$74BB`. That is what
consumes the stored target every other path in this routine computes.

**`$7476`** hands `$74BB` the *ball* instead — `$C0A3`/`$C0A7` — and then puts
three gates in front of requesting an action: `$C0FD` must be set, `$C0AB` must
have reached `$28`, and the roll in `$FFFE` must come in under the skill-scaled
threshold from `$7629`. Passing all three falls through into `$7496`, which sets
`$FFD2` to the `$C0FE` base with bit 0 forced on.

**`$742E`** is the defensive branch. A commit counter in `$C0F9` counts down
first and short-circuits to `$749E` while it runs. Once it expires, a roll of
`$07` or more just steers; below that the two entities' coarse fields are
compared and the result — `$01` when the opponent is below us, `$02` otherwise,
with equality taking `$02` because `cp` clears carry — is written to our facing
field and held for `$32` frames.

**`$7411`** releases immediately when the record field at `$0F` is not `$0D`
*and* the roll is under `$0A`; otherwise it waits for the `$C0FF` counter to
reach one.

## Scope

This ports the decision structure: the dispatch, the entry comparison, the
cleared state, the three target computations, the difficulty tables and the
three exits above.

`$74BB`, `$75CD` and `$74A8` are **not** included here because they were already
ported by the earlier Ghidra-backed AI work — `$74BB`'s four-pixel dead zone,
`$75CD`'s defender threshold and fourteenth-contact response, and `$74A8`'s
mode-2 target path all have existing tests in `allstar_cli.c`. An earlier note in
this session listed them as the next thing to port; that was wrong.

## The head, `$73C9..$7410`, added 2026-08-27

The coverage tool reported bank 1 `$73AD` as a single 251-byte routine. It is
three things, and only one of them was a real gap:

| range | what it is | was it ported |
|---|---|---|
| `$73AD..$73C8` | the second fourteen-pair coordinate table | yes, in `allstar_ai.c` |
| `$73C9..$7410` | the head that chooses among the exits | **no** |
| `$7411..$74A7` | the four exits documented above | yes |

### What the head does

```
$73CC  $C0FA == $02            -> $756C, the shot decision
$73D4  $C0FF == 0              -> $742E, defend
$73D7  $C0FF == 1              -> the commit test below
       otherwise               -> decrement it; exactly $25 falls into $7411
```

The commit test is the part worth spelling out. From the **exact** lane row
`$60`, with `$FFFB` under `$30`, the actor must be inside `$07B4`'s `$1E` box
**and outside its `$1A` box** — an annulus, not a disc. Miss any of that and it
falls back to the plain `$12` box; miss that too and it defends. Committing
loads `$C0FF` with `$2A` and requests an action through `$7496`.

So a commit lasts `$2A` frames, and the release check at `$7411` sees exactly
one frame of it — the one where the counter passes through `$25`. The test
walks a whole commit down and requires that to happen exactly once.

### The two tables

`$7367`'s `rst $10` picks between the fourteen-pair tables at `$7391` and
`$73AD` using the family byte, then `$736C` walks `$FFFE` in nineteen-unit
steps (`$13`) to choose one of the fourteen. Family `$82` never reaches the
dispatch at all: `$734C` special-cases it to the fixed centre spot `$54`/`$5D`,
because the inline pointer table at `$738D` only has two entries.

Both tables were already implemented in `allstar_ai_rom_route_target_732c`;
what was missing was any citation of the addresses, so the coverage tool could
not tell. They are now asserted pair by pair against the cartridge's bytes —
all 28 pairs match — and the `$A2`/`$F0` family thresholds are checked one
below each boundary.

Run:

```powershell
.uildllstar_port.exe --test-cpu-head
```

Mutation-checked twice: turning the annulus into a plain disc fails with
`$7401 annulus did not commit`, and comparing the release value before the
decrement rather than after fails with `$740D did not reach the release check`.
