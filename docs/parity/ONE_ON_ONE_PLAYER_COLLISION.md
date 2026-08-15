# One-on-One player collision parity

## Verified ROM path

Ghidra recovers bank 1 `$6E3C` as the directional player-pair gate used by
the four movement callbacks `$6BAD/$6BBA/$6BC7/$6BD4`. It calls `$6EC0` for
player `+$06` horizontal separation and `$6EEA` for player `+$15` ground-Y
separation. The other player is selected through fixed-bank `$0773` and the
active-player byte `$C187`.

The helpers use unsigned byte subtraction and return a contact side:

| Routine | Negative side | Positive side | Outside |
|---|---|---|---|
| `$6EC0` X | `4` for `-11..-1` | `3` for `0..+10` | `0` |
| `$6EEA` Y | `1` for `-6..-1` | `2` for `0..+5` | `0` |

`$6E3C` probes four pixels along the requested direction. A zero result on
either axis allows movement because the players do not overlap. A primary
side matching the direction from which the player is moving also allows
movement because the actor is moving away. Otherwise the overlap blocks the
movement callback. Direction priority is right, left, up, then down; game
modes `$02/$03` bypass the pair gate.

| Motion | Primary probe | Side that means “moving away” |
|---|---:|---:|
| Right | X `+4` | `4` |
| Left | X `-4` | `3` |
| Up | Y `-4` | `2` |
| Down | Y `+4` | `1` |

## Native implementation

`allstar_one_on_one_rom_player_x_side_6ec0` and
`allstar_one_on_one_rom_player_y_side_6eea` reproduce the byte thresholds,
including their asymmetric endpoints. The deterministic native test checks
all eight in/out boundaries. `allstar_one_on_one_rom_player_pair_blocks_6e3c`
implements the four probe branches and the `$02/$03` bypass.

The live scene applies the gate before each human axis move. CPU intent is
captured from its Ghidra-backed controller displacement, then each axis is
accepted or rejected through the same gate. Native center X is converted back
to ROM `+$06` by subtracting eight; native ground Y already matches `+$15`.

`trace_one_on_one_player_collision.lua` drives the original cartridge with
rightward input and controlled player coordinates. It observes `$C16B=1` for
an overlapping player ahead, then `$C16B=0` for motion away and for vertical
separation.

## Coverage limit

The detector and side selection are verified. `$6E3C` remains a whole-routine
candidate because the native scene consumes the Boolean result directly
instead of reproducing the ROM's persistent player `+$0C` and shared `$C16B`
latch ownership. Contact-hit actions, displacement reactions, velocity
cancellation, recovery duration, and re-entry state remain separate open
requirements.
