# One-on-One asset and OAM parity

## Asset-pack v8 layout

The One-on-One renderer no longer includes extracted court/player/ball art in
the executable. `--build-assetpack` reads these regions from the user's ROM,
validates every decoded length and index, and stores decoded tiles plus the
small composition maps in a version-8 pack. Version 8 also appends the focused
One-on-One `$05/$0D/$0C/$0F/$0E` audio programs; the graphics payload itself
is unchanged.

| Asset | ROM source | Decoded result |
|---|---|---:|
| Player frame maps A | bank 1 `$4000..$411F` | 16 × 18 indices |
| Player frame maps B | bank 1 `$4120..$42AB` | 22 × 18 indices |
| Player frame maps C | bank 1 `$42AC..$4437` | 22 × 18 indices |
| Ball/shadow descriptors | bank 1 `$4438..$44B7` | 32 four-byte pairs |
| Player tiles A | bank 1 `$44B8..$4DE7` | 147 2bpp tiles |
| Player tiles B | bank 1 `$4DE8..$582A`, `$050F` RLE | 211 tiles |
| Player tiles C | bank 1 `$582B..$62A5`, `$050F` RLE | 205 tiles |
| Ball/shadow tiles | bank 1 `$62A6..$640E`, `$1FFA/$20BA` | 42 source tiles, interleaved with blank VRAM tiles by the ROM |
| Net/HUD tiles | bank 3 CPU `$793F..$7A22`, file `$F93F..$FA22`; `$1FFA/$2021` and `$2219` | 17 tiles at VRAM `$9600`, signed BG IDs `$60..$70` |
| Court tiles | bank 3 CPU `$7A23..$7E47`, file `$FA23..$FE47` | 86 tiles |
| Court map | bank 3 CPU `$7E48..$7F68`, file `$FE48..$FF68` | 640 bytes in a zero-filled 32×32 map |

Fixed-bank `$050F` uses the stream's first byte as an escape marker. Literal
bytes copy directly; `marker,count,value` repeats the value; `marker,0` ends.
The builder rejects a wrong terminator offset, output length, or tile index.

## Player composition

`$2933` assigns P1's nine OAM tiles, `$293D` assigns P2's, and both continue
through `$2945/$2A2B`. Actions `$00..$07`, `$08..$0F`, and `$10..$17` select
the A/B/C tile and frame families. Each frame has three sprite rows. Within a
row, normal traversal is `0,3,1,4,2,5`; horizontal flip is
`2,5,1,4,0,3` with each tile mirrored.

`allstar_renderer_rom_player_tiles_2945` produces this exact 18-tile order.
Native deterministic tests cover all three family offsets and both traversal
directions. The renderer consumes the returned tiles directly instead of
recreating the Game Boy's temporary VRAM cache.

## Ball and shadow composition

`$6945` derives phase from `ball_x & 7`. When the ball is behind its owner,
the phase moves between halves by four and aligned OAM X shifts by four. Ball
OAM Y is `ball_y-ball_z`; shadow Y is `ball_y`.

For shot gather and release, `$7F37` supplies those inputs. Its exact `$7FC7`
tables are `[+7,-2],[+10,-2]` for action `$0A` and `[+7,+10],[-2,+8]`
for the other shot family. Display frames `$00` and `$0C` suppress the
separate ball because the player frame already includes it.

During ordinary held-ball movement, `$6F2A` runs after `$7F37` and is the
final placement. Actions `$01/$04/$08/$0B/$10/$13` have distinct X/Y offsets;
actions `$10/$13` use player `+$02` bit 4 to select the side. Record index
`+$03` selects height from `$6FEA`:
`0C,0C,0C,08,04,00,04,08,0B,01,01,01`.

| Height | `$6A5C` descriptor family |
|---:|---|
| `$00..$07` | ground `$4498..$44B7` |
| `$08..$1E` | middle `$4478..$4497` |
| `$1F..$FF` | high `$4458..$4477` |

`trace_one_on_one_assets.lua` verifies the cartridge's initialized VRAM
signatures and phase-three OAM pairs:

| Region/state | Live result |
|---|---|
| Ball VRAM `$8240`, 1,344 bytes | sum `$5B35`, weighted `$D064`, xor `$DB` |
| Net tiles `$9600`, 272 bytes | sum `$71F7`, weighted `$8DFE`, xor `$35` |
| Court tiles `$9000`, 1,376 bytes | sum `$A549`, weighted `$9BEE`, xor `$3F` |
| Court map `$9800`, 640 bytes | sum `$1C9B`, weighted `$25B0`, xor `$71` |
| Phase 3, Z `$07` | ball `$2E/$30`, shadow `$5C/$60` |
| Phase 3, Z `$08` | ball `$2E/$30`, shadow `$5E/$60` |
| Phase 3, Z `$1F` | ball `$2E/$30`, shadow `$48/$4A` |

The loader attribution matters: One-on-One enters through `$0B9A` and selects
the `$04B1` A=1 court pair. Fixed `$2243` belongs to another mode and receives
no One-on-One coverage credit.

## Runtime boundary

The repository retains only extraction logic, composition metadata schemas,
and a procedural fallback. Actual One-on-One tile pixels are loaded from the
user-created pack at runtime; ROM files and generated packs remain outside
version control.
