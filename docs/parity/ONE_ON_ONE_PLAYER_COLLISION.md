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

The live scene applies movement only when `$6A8C` loads a normal animation
record. `allstar_one_on_one_rom_player_move_6b72` clears the record's contact
latch, processes right/left/up/down callbacks, applies four pixels when clear,
and sets the latch when blocked. CPU intent is captured first and then routed
through the same record-boundary callback. Native center X is converted back
to ROM `+$06` by subtracting eight; native ground Y already matches `+$15`.

`trace_one_on_one_player_collision.lua` drives the original cartridge with
rightward input and controlled player coordinates. It observes `$C16B=1` and
zero displacement for an overlapping player ahead, then `$C16B=0` and exactly
four pixels for motion away and for vertical separation. The action byte is
unchanged in all three callbacks.

## Charging, blocking, and AI response

`$100F` calls fixed `$2C50` before the movement path. `$2CCA` therefore
consumes the prior update's `+$0C` latch. A clear/protected contact reloads
`+$0D=$19`; continuous eligible contact decrements it, and zero calls `$0AC5`.
The possession owner's `+$16` selects one exact spacing: defender X at owner
X plus 12, defender Y at owner Y minus 8, or defender X at owner X minus 12.
An owner offender is charging; a defender offender is blocking. `$20F7`
restarts possession for the player opposite the offender.

`trace_one_on_one_contact_rules.lua` verifies charging, blocking, and the
protected `$0A` shot action. The native C mirrors player-one-first priority,
the unsigned 25-count lifecycle, failed-alignment latch clear, and live
possession restart.

Bank-1 `$75CD` is the CPU's only body-contact response. It does not assign a
hit action. It uses skill thresholds `$BE/$AA/$96`; a ballhandler reroutes and
forces A on the fourteenth qualified response, while a defender stores its
position and holds that target for ten counts. `allstar_ai_rom_contact_response_75cd`
implements and deterministic-tests those outcomes.

There is no player recoil, knockback, or velocity object in this ROM path.
Blocked callbacks simply return before their direct coordinate write, and the
next normal animation record clears/recomputes the latch.
