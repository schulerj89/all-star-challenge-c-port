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

## Scope

This ports the decision structure: the dispatch, the entry comparison, the
cleared state, the three target computations and the difficulty tables. The
routine also calls `$75CD`, `$74BB`, `$7476` and `$7496`, which remain unported;
those are named as exits rather than reproduced.
