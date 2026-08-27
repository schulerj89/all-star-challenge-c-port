# The Caption Script, `$07E3`

Added **2026-08-27**. Ported from `$07DE..$0801` and the `$0802` script it
walks, together with `$0D2B`, `$6E1B`, `$3264` and `$327F`.

## What it is

`$07E3` is the whole screen-caption system. It takes a layout index in A,
skips that many `$FF` markers from `$0802`, and then walks four-byte records —
row, column, and a pointer to a tile stream — handing each to `$06C0` until the
next byte is `$FF`. `$07DE` is the same thing with a `$047E` clear in front.

The index is **one-based**, because `$0802` is itself a marker.

Twenty-five layouts cover every prompt in the cartridge:

| layout | what it says |
|---|---|
| 1 | SELECT / PLAYER |
| 2 | SELECT / YOUR / OPPONENT |
| 3–6 | SELECT / FOUR or TWO / PLAYERS or OPPONENTS |
| 7–8 | SELECT / FINAL / PLAYER or OPPONENT |
| 9 | SHOTS ATTEMPTED / SHOTS MADE |
| 10 | CHOOSE / TEN / POSITIONS |
| 11 | SELECT / AGAIN |
| 12 | TIME / SHOTS ATTEMPTED / SHOTS MADE / SCORE |
| 13–14 | the bracket's GAME 1 … GAME 6 |
| 15–18 | YOUR PLAYERS / OPPONENTS / PLAYER n CHOSE, with CONTINUE |
| 19–20 | PLAYER n |
| 21 | CONGRATULATIONS / YOU ARE / THE CHAMPION |
| 22 | EXTENDED TIME / n MINUTE |
| 23 | PLAYER n GAMES WON / COMPUTER GAMES WON |
| 24–25 | WAITING / FOR / PLAYER n |

## Two details worth recording

**The streams are tile indices, not ASCII.** The last tile of a stream carries
bit 7 — `$0968` is `53 45 4C 45 43 D4`, "SELEC" then "T" with bit 7 — and
several streams open or end on codes that are not characters at all: `$0A15` is
"PLAYER " followed by tile `$82`, and `$09D8` starts with the bracket tile
`$63`. Storing them as bytes keeps `$06C0`'s behaviour instead of guessing a
character set.

**D is the column and E the row.** `$0558` puts D straight into the
destination's low byte and scales E by the map stride. The records store E
first, so a record reads row-then-column even though `$06C0` takes them as DE.
Reading the pair the other way puts the bracket's twelve GAME labels on one row
instead of down one column, which is how the mistake shows up.

Because this is ROM data, the script is extracted into the asset pack rather
than embedded in the source, alongside the animation records and the title
song. `ALLSTAR_ASSET_VERSION` is now 19.

## The four routines alongside it

**`$0D2B`** hands the court to a shooter. Called with 1 or 2 from `$0D14` and
`$0D21`, it parks the value in `$C179`, mirrors it into `$FFDA`, raises `$FFE7`,
`$FFE6` and `$C12C`, clears `$C0FD` and `$C145`, waits four frames through
`$2D08` and turns objects back on with `set 1,[hl]` on LCDC.

**`$6E1B`** is what reads `$FFDA` back. It copies it to `$FFCF` and seeds one
of two entity slots with the same fixed spot `$5C`/`$4C`/`$04` — shooter one
takes `$FFA2..$FFA4`, everyone else `$FFBB..$FFBD`. The compare is `dec a`
followed by `jr nz`, so **only an exact 1** takes the first slot. The test
asserts the pair end to end: `$0D2B` parks a shooter, `$6E1B` picks it up.

**`$3264` and `$327F`** are the same per-frame loop over different halves of
the eight voices, and both walk **downwards**: `$3264` covers channels 3 to 0
and `$327F` channels 7 to 4. A channel whose `$DD7F` byte is zero is skipped
entirely; otherwise it runs `$32E9`, `$347B`, `$32B8`. `$3264` stops when bit 7
of C is set — after channel 0 wraps to `$FF` — while `$327F` stops on a plain
`cp $04`, so neither can run into the other's half.

## Verification

```powershell
.\build\allstar_port.exe --test-captions
```

The test checks the terminator handling and the voice-loop orders without a
ROM, then — when `build\allstar.assetpack` is present — walks all 25 layouts
and requires every record to be non-empty and bit-7 terminated, with layout 1,
the bracket and the champion banner checked against their exact ROM pointers,
rows and columns.

Mutation-checked twice: reading row and column from the same record byte fails
with `layout 1 diverged (... row 7 col 7)`, and treating any nonzero shooter as
shooter one fails with `$6E1B put shooter 2 in slot $FFA2`.
