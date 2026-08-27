# Mode Menu, Voice Switch, and a Data Correction

Added **2026-08-26**.

## `$038F`, the mode-select menu

Five entries in `$FF8F`, wrapping both ways. The input masks use the
cartridge's own packing (bit 0 A, 1 B, 2 Select, 3 Start, 4 Right, 5 Left,
6 Up, 7 Down), **not** `AllStarButtonMask`:

| mask | at | meaning |
|---|---|---|
| `$08` | `$03D4` | Start confirms |
| `$C3` | `$03F5` | A, B, Up or Down moves |
| `$42` | `$03F9` | B or Up steps backward |

Three separate things refuse the confirm: a two-player game sitting on mode
`$04`, a set `$FFEC`, or Start not being newly pressed. **Two players cannot
select the tournament.**

The interesting part is `$03DD`: when a two-player game confirms mode `$01` or
`$03`, that mode is written into `$C18B` — the same link-game flag the pause
handler reads at `$2BE7`. Free Throw and Accuracy are exactly the two modes
whose postgame screens run the `$F0` link handshake (`$1121` and `$1209`), so
this is where the cartridge decides a session is a link game. That is the sixth
and last link-cable site, and it ties the other five together.

A one-player game skips the menu music at `$DD73`; `$03AE` clears both pads'
new-input bytes and primes both held-input bytes to `$FF`.

## `$32B8` and `$32E9`, the voice context switch

The sound driver keeps one working voice at `$DE28..$DE2D` and a per-channel
copy of each field in six arrays eight bytes apart from `$DDBF`, indexed by BC.
`$32B8` stores the working voice into a channel's slots and `$32E9` loads it
back; the two are exact mirrors.

The field order is **not** sequential:

| # | scratch | array |
|---|---|---|
| 0 | `$DE2A` | `$DDBF` |
| 1 | `$DE2B` | `$DDC7` |
| 2 | **`$DE2D`** | `$DDCF` |
| 3 | **`$DE2C`** | `$DDD7` |
| 4 | `$DE28` | `$DDDF` |
| 5 | `$DE29` | `$DDE7` |

The third and fourth entries swap. Reproduce that or two of the six fields land
in each other's slots — which would survive a round trip through both routines
and only show up as wrong sound.

## Correction: `$7391` and `$73AD` are data, not routines

Earlier reporting called `bank1 $73AD` "251 bytes, the largest unported routine
left". That was wrong, and the error was in the analysis, not the port.

`$738D` is a two-entry pointer table holding `$7391` and `$73AD`. Both targets
are **28-byte tables of fourteen coordinate pairs**:

```
$7391  (84,90) (98,28) (98,48) (60,0C) (74,9C) (94,0C) (98,70)
       (8C,9C) (88,14) (60,9C) (98,5C) (70,0C) (98,90) (98,54)
$73AD  (70,34) (8C,60) (84,28) (68,5C) (74,80) (64,48) (74,68)
       (64,80) (64,24) (88,50) (64,70) (84,3C) (84,78) (70,50)
```

`$7367` picks one pair by bucketing `$FFFE` in steps of `$13` over at most
`$0D` entries, and writes it to `$C101`/`$C102`. The X values cluster in
`$60..$98` and the Y values in `$0C..$9C`, so these are court positions — the
same shape as the fifty-pair `$6DB7` table the H-O-R-S-E work already uses.

Real code does start at `$73C9`, inside what the trace called `$73AD`. Its true
entry point is not `$73AD`, so the recursive-descent inventory attributed 28
bytes of data and 223 bytes of code to one wrong address.

### What this means for the inventory numbers

The "310 routines / 16,797 bytes of executable code" figure has now been shown
to include at least three data misclassifications, all of the same kind — a
pointer-table target that holds data rather than code:

- the four `$7DD1` sub-tables in bank 1 (35 bytes)
- `$7391` (28 bytes)
- the first 28 bytes of `$73AD`

That is roughly 91 bytes, about half a percent, so the coverage percentages are
close but not exact. Every such case is a `rst $10` or `rst $08` table whose
entries are data; a stricter plausibility check on table targets would catch
them, at the cost of possibly rejecting real code.
