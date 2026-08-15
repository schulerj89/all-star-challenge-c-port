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
no-ball right/left/up/down motion `$11/$11/$09/$02`; direction-dependent idle
`$0D`; middle-family defensive jump `$0C`; and post-jump `$0F` steal. It also
asserts the six-frame record reload seen at `$6A8C`.

## `$782E` action selection

Ghidra recovers the complete selector called immediately before `$6A8C`.
It runs only when animation timer `+$04` is one, reaction lock `+$17` is zero,
and `$0A78` accepts the current action. Direction override `+$14` wins over
input `+$07`; with neither set, prior direction `+$10` chooses the idle pose.
Player `+$09` selects the no-ball member of each pair.

| Direction | Held-ball moving | No-ball moving | Held-ball idle | No-ball idle |
|---|---:|---:|---:|---:|
| Right | `$10` | `$11` | `$13` | `$15` |
| Left | `$10` | `$11` | `$13` | `$15` |
| Up | `$08` | `$09` | `$0B` | `$0D` |
| Down/default | `$01` | `$02` | `$04` | `$06` |

Right sets player `+$02` bit 4; all other branches clear it. A changed action
is reset through `$6C90`. Bank 1 `$70FD` separately preserves the same
eight-action family for defense jumps: `$05`, `$0C`, or `$14`.

## Native implementation

`AllStarRomAnimationState` mirrors the five animation fields, and
`allstar_one_on_one_rom_animation_tick_6a8c` implements record timing, looping,
completion, and action transitions. The live scene calls
`allstar_one_on_one_rom_select_movement_action_782e` at the same record
boundary, retaining prior direction and the horizontal-flip bit per player.
It uses `$70FD`'s family selector for defense jumps, `$2B14`'s family selector
for steals, and passes the resulting display-frame byte to the renderer.

Asset-pack version 4 retains the 24 `AllStarRomAnimationAction` entries. Building a
pack reads the `$6C60` pointers and their control lists directly from the
user-supplied ROM. It also extracts the three player tile families and all 60
18-index frame maps used by fixed `$2933/$293D->$2945->$2A2B`. The repository
contains only a small native control-table and procedural-art fallback; it
does not contain the extracted One-on-One sprite sheets.

## Contact and recovery caller correction

Ghidra shows no contact-hit or rebound-pickup action setter. Ordinary contact
returns from `$6BAD/$6BBA/$6BC7/$6BD4` before displacement and leaves the
action untouched. `$2AE2/$2B88` recovery changes possession fields without
touching animation fields `+$00..+$04`; normal `$782E/$6A8C` selection resumes.
The native scene now preserves both behaviors instead of forcing invented
animations.

Player composition, court art, and `$6945/$69F5` eight-phase ball/shadow OAM
are documented in `ONE_ON_ONE_ASSETS.md`. `$782E` is verified for its complete
movement/idle selector; audio command sequencing remains tracked separately
from this gameplay/animation focus.
