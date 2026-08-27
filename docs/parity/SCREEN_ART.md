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

- **Portrait**: `$4199` fills the cell map with a plain 1..24 and `$41C7` draws
  it four wide by six tall, so it is simply stream tiles 1 through 24.
- **Logo**: `$41E7` is more interesting. It walks a per-player list at `$42BD`,
  terminated by `$FE`, whose entries name cells to leave **blank**; every other
  cell takes a running counter plus `$18`, or `$19` when that player's `$42A2`
  byte is `$FF`. `$421D` then draws the first sixteen of those cells four by
  four. Across the 27 players `$42BD` blanks 42 cells, and twelve players have
  none at all.

## Verification

The migration was checked by composing every image from the ROM in a scratch
decoder and diffing it against the bitmap it replaced:

| | result |
|---|---|
| all five screen bitmaps | **pixel-identical** |
| 27 team logos | **27 of 27 identical** |
| 27 player portraits | **20 of 27 identical** |

The seven portraits that differed were **wrong in the committed header**, not
in the extraction — the method reproduces the other twenty exactly with a
straight identity mapping. Player 0 was off by 182 pixels, which is precisely
the amount the two roster screenshots changed by:

```
58 of 60 --dump-screenshots captures are byte-identical before and after;
the two that changed are the player-select screens, both by 182 pixels.
```

So the migration also corrects seven portraits that had been wrong.

```powershell
.\build\allstar_port.exe --test-rom-art
```

The test pins the `$04EF` records, that slot 5 is empty and slot 8 comes from
bank 1, that every map stays inside its own tile stream, that the settings
variants share tiles but not maps, and the two cell-map rules above including
the 42-blank total.

`ALLSTAR_ASSET_VERSION` is now 20; rebuild `allstar.assetpack` from the ROM.
