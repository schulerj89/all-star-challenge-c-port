# The Title Screen Music, `$029C` / `$35B6`

Added **2026-08-27** after a report that the title audio did not sound right.

## What the cartridge does

`$02A7` posts `$81` into the sound-command mailbox `$DD73`. The once-per-frame
handler at `$3014` reaches `$30A0`, sees bit 7, masks it off to song index
`$01`, and latches three parameters:

| table | index | song `$01` |
|---|---|---|
| `$3849` | word | program `$3B25` |
| `$3823` | word | offsets `$3AAB` |
| `$386F` | byte | update skip `$07` |

From then on `$3264` runs voices 3..0 through `$32E9` -> `$347B` -> `$32B8`
every frame, and the skip counter at `$DD7C` makes one frame in seven a
non-update, so the sequencer ticks at 6/7 of the frame rate.

Two routines own the stereo image:

- **`$35B6`** starts a voice. It takes bits 2-3 of the voice's instrument
  descriptor as a pan code, clears that voice's two NR51 bits, and ORs in the
  matching entry of `$3777` / `$377B` / `$377F` / `$3783`. All four tables are
  the same pair of bits shifted by the voice index: `0` neither side, `1`
  right, `2` left, `3` both.
- **`$3587`** rests a voice. It ANDs the voice's two NR51 bits back off through
  `$3787`, which is how a rest is silenced — the frequency registers are left
  exactly as they were.

For the title theme that resolves to:

| NR51 | square 1 | square 2 | wave | noise |
|---|---|---|---|---|
| `$ED` | **right** | **left** | both | both |
| `$CC` | resting | resting | both | both |

## What was wrong

Nothing in the decode. Measured against the cartridge over 901 audio frames —
see the verification section — the port's note stream, note timing, envelopes,
frequencies, rests, noise and the wave channel's per-frame `$3244` vibrato all
matched exactly, with zero divergences.

The port simply threw the routing away. `AllStarRomMusicFrame` had no panning
field, and `generate_rom_music` summed all four voices into one value and wrote
it to both output channels. The cartridge's two lead voices, which sit on
opposite sides, were collapsed on top of each other in the middle.

The frame now carries the NR51 byte the cartridge would be holding, decoded the
way `$35B6` decodes it, and the renderer accumulates a left and a right mix with
its own high-pass per side. Measured on the rendered output, left/right
correlation over the first twenty seconds went from 1.000 (identical channels)
to 0.635.

## Verification

`tools/emulator/trace_title_music.lua` boots the cartridge in Mesen, waits for
`$02AC`, and logs every APU register write tagged with the audio frame:

```powershell
$mesen = '<path to Mesen.exe>'
$script = (Resolve-Path '.\tools\emulator\trace_title_music.lua').Path
$rom = '<path to the ROM>'
Start-Process -FilePath $mesen -Wait -WindowStyle Hidden `
  -ArgumentList "--testRunner --enableStdout --timeout=120 `"$script`" `"$rom`"" `
  -RedirectStandardOutput title_apu.txt
```

Diffing that capture against the decoded program over its first 901 frames:

| quantity | mismatches |
|---|---|
| square 1 frequency | 0 / 901 |
| square 2 frequency | 0 / 901 |
| wave frequency | 0 / 901 |
| square 1 envelope | 0 / 901 |
| noise polynomial | 0 / 901 |
| noise on/off | 0 / 901 |
| **NR51 routing** | **0 / 901** |

The routing row is the one that was newly added; the others were already exact
and are recorded here so a later change cannot quietly break them.

In-tree:

```powershell
.\build\allstar_port.exe --test-title-music
.\build\allstar_port.exe --export-title-music build\allstar.assetpack title.wav
```

The test checks the `$3777`/`$377B`/`$377F`/`$3783` mapping for all four voices
and all four pan codes without needing a ROM, then — when
`build\allstar.assetpack` is present — checks song `$01`'s three parameters, the
exact NR51 the cartridge holds on fourteen captured frames, and the invariants
that square 1 is always hard right while it sounds, square 2 always hard left,
and a resting voice contributes no bits at all.

Mutation-checked twice. Ignoring the pan code and centring every voice — the
behaviour being fixed — is rejected by the extractor's own frame-0 invariant
before a pack is even written. Routing resting voices as well (ignoring `$3587`)
builds a pack successfully and is then caught by the test at frame 14, which the
cartridge routes `$CC` and the mutant routes `$ED`.

## Asset pack version

The frame gained a byte, so `ALLSTAR_ASSET_VERSION` went from 17 to 18. Rebuild
`allstar.assetpack` from the ROM; an older pack is rejected at load.
