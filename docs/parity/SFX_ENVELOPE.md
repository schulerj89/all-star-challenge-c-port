# Sound-Effect Envelopes, `$2AB5` and NR12

Added **2026-08-27** after a report that the cue when changing players did not
sound right.

## The cue

Bank 2 `$411D` calls `$2AB5` every time the roster selector moves between
players. `$2AB5` is two instructions:

```
$2AB5  ld a,$0F
$2AB7  jp $2F88
```

`$2F88` maps command `$0F` through the table at `$2FB0` to **program `$07`**
with a **`$19`-frame priority window**, then writes `$87` — the program index
with bit 7 set — into `$DD72` for the engine to start.

Captured from the cartridge (`tools/emulator/trace_navigation_sfx.lua`), the
whole cue is a **single channel-1 trigger**:

```
NR10=$08  NR11=$88  NR12=$F1  NR13=$B1  NR14=$BF
```

and then nothing more on channel 1 until frame 45, where the title song takes
the channel back with its own `$069E`/`$F7` note.

Two of those bytes decide how it sounds:

- **NR14 = `$BF`** — bit 7 triggers, and **bit 6 is clear**, so no length
  counter runs. The note is not cut; it fades.
- **NR12 = `$F1`** — starting level 15, direction *decreasing*, pace 1. The
  level drops one step every 1/64 s, so the cue reaches silence after fifteen
  steps: **234 ms**.

NR10 = `$08` looks like a sweep but its pace field is zero, so it is inert and
the pitch holds at `$07B1`.

## What was wrong

`generate_rom_program` computed the square volumes **once**, before the sample
loop:

```c
volume1 = (float)(program->square1_envelope >> 4) / 15.0f;
```

and never touched them again. The noise channel already stepped its envelope
correctly; the two square channels did not. So every square cue in the game
played at a flat, full level for the whole length of its program.

For this cue that meant **402 ms of constant full-volume tone** where the
cartridge plays a **234 ms decay** — nearly twice as long, with no shape at
all. That is what turns a short blip into a harsh sustained beep.

The fix tracks the volumes as DMG levels, re-arms them on every trigger, and
steps them through the same `step_envelope` the music renderer and the noise
channel already used.

This was not specific to `$0F`. Every square-based cue was affected: the
confirm `$0E`, dribble `$0C`, score `$05`, shoe squeak `$0D`, horse letter
`$07`, accuracy result `$02`, and both free-throw cues.

## Verification

```powershell
.\build\allstar_port.exe --test-sfx-envelope
```

The test pins the NR12 curve step by step — fifteen decreasing steps at 1/64 s
for `$F1`, a pace of zero holding flat, bit 3 rising and clamping at 15 — then
checks the `$2FB0` mapping and the exact trigger bytes against the capture.

Most importantly it renders the cue **through the path the game plays** and
requires it to be loud at the attack and silent once the envelope has run out.
That last part matters: an earlier version of this test only exercised the
envelope helper, and the flat-volume renderer passed it. Rendering is what
catches the real defect.

Mutation-checked by restoring the flat-volume behaviour, which fails with
`the cue still sounds at peak 6200 after its envelope ran out`.
