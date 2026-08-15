# One-on-One animation dispatcher parity

## Verified ROM path

Ghidra recovers bank 1 `$6A8C` as the per-player animation dispatcher. It
selects the active player structure through `$782E`, resolves the action byte
through `$6C59` and the 24-entry pointer table at `$6C60`, indexes three-byte
records with player `+$03`, and decrements the duration at `+$04` before
processing a record. A normal record stores its duration in `+$04`, its
display frame in `+$01`, sets the new-frame flag in `+$02`, and increments the
record index. Control bit 0 loops to record zero; bit 1 changes action and
resets the record state; `$FF` marks completion.

The reviewed player fields are:

| Offset | Meaning |
|---:|---|
| `+$00` | action |
| `+$01` | display frame |
| `+$02` | status/new-frame flags |
| `+$03` | animation record index |
| `+$04` | record duration timer |

## `$6C60` action map

The asset pack stores every pointer and its decoded control records. The
pointer map recovered from Ghidra and extracted from the user ROM is:

| Actions | Record pointer(s) | Verified role/effect |
|---|---|---|
| `$00..$04` | `$6787,$678B,$67A4,$67BD,$67BD` | directional movement/held-ball family; exact records extracted |
| `$05` | `$67D6` | 12 six-frame defensive-jump records, then action `$06` |
| `$06` | `$67FC` | frame `$0E`, loop |
| `$07` | `$6800` | 15-frame steal pose, then action `$06` |
| `$08,$09` | `$6805,$681E` | directional movement family; exact records extracted |
| `$0A` | `$6837` | 67-frame shot, then action `$0D` |
| `$0B` | `$685D` | exact looping record family |
| `$0C` | `$6876` | 12 six-frame defensive-jump records, then action `$0D` |
| `$0D` | `$689C` | no-ball idle frame `$11`, loop |
| `$0E,$0F` | `$68A0` | 15-frame pose, then action `$0D` |
| `$10,$11` | `$68A5,$68BE` | directional movement family; exact records extracted |
| `$12` | `$68D7` | 67-frame shot, then action `$0D` |
| `$13` | `$68FD` | held-ball idle frames `$0C,$0D,$0E,$0D`, loop |
| `$14` | `$6916` | 12 six-frame defensive-jump records, then action `$15` |
| `$15` | `$693C` | frame `$11`, loop |
| `$16,$17` | `$6940` | 15-frame pose, then action `$15` |

`trace_one_on_one_animation.lua` establishes action semantics using controlled
live input rather than names inferred from similar-looking data. It observes
held-ball idle `$13`; right/left/up/down held-ball motion `$10/$10/$08/$01`;
no-ball idle `$0D`; right no-ball motion `$11`; defensive jump `$14`; and the
post-jump `$17` steal action. It also asserts the six-frame record reload seen
at `$6A8C`.

## Native implementation

`AllStarRomAnimationState` mirrors the five animation fields, and
`allstar_one_on_one_rom_animation_tick_6a8c` implements record timing, looping,
completion, and action transitions. One-on-One stores one state per player,
ticks it at 60 Hz, uses `$2B14`'s action-family selector for steals, enters the
reviewed `$14` defensive-jump family, and passes the resulting display-frame
byte to the renderer.

Asset-pack version 3 adds 24 `AllStarRomAnimationAction` entries. Building a
pack reads the `$6C60` pointers and their control lists directly from the
user-supplied ROM. The repository contains only a small native control-table
fallback; it does not contain extracted sprite sheets.

## Deliberate coverage limits

This checkpoint does **not** claim the remaining graphics work. The renderer
still maps ROM display-frame IDs onto its existing compiled derived art. Player
tile regions, frame tilemaps, OAM composition, court tiles/tilemap, and ball
sprite extraction remain open. Full directional walk/dribble action selection
also remains open until every controller branch selecting `$00..$11` is mapped
and integrated, even though their record lists are now extracted exactly.
