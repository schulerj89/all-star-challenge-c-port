# Screen and Portrait Art, `$04B1` and `$2D4F`

Added **2026-08-27**. This retired 624 KB of ROM-extracted art that had been
committed as C headers, which `AGENTS.md` forbids:

> **Data Boundary**: NEVER commit ROM binaries, raw PRG/CHR dumps, extracted
> copyright sprite sheets, or private binary blobs.

The five headers held five full-screen 160x144 bitmaps and, in
`allstar_player_art.h`, 27 NBA player portraits and 27 team logos. They are now
decoded from the user's own ROM into the asset pack like every other asset.

## The screens, `$04B1`

`$04B1` takes a screen index in A, reads a four-byte record from `$04EF` — a
tile stream pointer then a tilemap pointer, both bank 3 — and RLE-decodes them
through `$050F` into `$9000` and `$9800`.

| index | screen | tiles | map |
|---|---|---|---|
| 0 | title | `$406D` (231) | `$4CE6` |
| 1 | roster frame | `$7A23` (86) | `$7E48` |
| 2 | mode menu | `$4E3B` (128) | `$5514` |
| 3 | settings, One-on-One | `$5612` (150) | `$5E3D` |
| 4 | settings, Free Throw | `$5F21` (229) | `$6B9B` |
| 5 | *unused* | — | — |
| 6 | settings, Accuracy | `$5F21` | `$6CC5` |
| 7 | settings, Tournament | `$5612` | `$6DFB` |
| 8 | copyright | bank 1 `$640F` | bank 3 `$4000` |

Slot 8 is not in the `$04EF` table: `$0271` loads its pair directly, and it is
the only screen whose tiles come from bank 1.

Two things fell out of this. **Each map indexes its own decoded stream
directly** — no signed `$8800` window, even where a screen has more than 128
tiles. And **`$22FC` reaches the settings screens as mode + 3**, so each of the
four modes has its own map; three of them share tiles with another and differ
only in the map. The port had been approximating that with two backgrounds
picked by mode, which could not represent the Accuracy and Tournament variants.

## The portraits, `$2D4F`

Bank 2 `$418D` indexes a pointer table at `$2D4F` by the roster entry and
RLE-decodes one stream per player — about 40 tiles.

- **Portrait**: `$4199` fills the cell map with a plain 1..24 — but that is
  not the whole story. `$41B0` reads a per-player byte from `$42A2` and, unless
  it is `$FF`, **blanks the cell that byte names and decrements every cell after
  it**, so the tiles past the gap all shift down by one. Exactly seven of the 27
  players take that path. `$41C7` then draws the map four wide by six tall.
- **Logo**: `$41E7` walks a per-player list at `$42BD`, terminated by `$FE`,
  whose entries name cells to leave **blank**; every other cell takes a running
  counter plus `$18`, or `$19` when that player's `$42A2` byte is `$FF`.
  `$421D` then draws the first sixteen of those cells four by four. Across the
  27 players `$42BD` blanks 42 cells, and twelve players have none at all.

Note that the same `$42A2` byte drives both: `$FF` means no portrait patch and
a logo base of `$19`; anything else patches the portrait and uses `$18`.

## Verification

The migration was checked by composing every image from the ROM in a scratch
decoder and diffing it against the bitmap it replaced. **All five screens, all
27 logos and all 27 portraits are pixel-identical**, and

```
60 of 60 --dump-screenshots captures are byte-identical before and after.
```

That took two passes, and the first one was wrong in an instructive way. The
initial extractor treated the portrait map as an unconditional 1..24 and
reproduced only 20 of 27 portraits. Seven differed, and the tempting conclusion
— that the committed bitmaps were wrong — was itself wrong. The seven turned
out to be exactly the seven players whose `$42A2` byte is not `$FF`, which is
the set `$41B0` patches. The extractor was incomplete; the old bitmaps were
right all along.

The lesson is worth keeping: a clean correlation between the failures and one
ROM byte is a much better hypothesis than "the old data was wrong", and it is
cheap to check.

```powershell
.\build\allstar_port.exe --test-rom-art
```

The test pins the `$04EF` records, that slot 5 is empty and slot 8 comes from
bank 1, that every map stays inside its own tile stream, that the settings
variants share tiles but not maps, and both cell-map rules — including that
`$41B0` patches exactly seven portraits, that each patched map is untouched
before its gap and shifted after it, and that `$42BD` blanks 42 logo cells.

`ALLSTAR_ASSET_VERSION` is now 20; rebuild `allstar.assetpack` from the ROM.
