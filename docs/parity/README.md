# Parity documents

One document per ROM subsystem. Each records what the cartridge's own code
does, what the port does about it, and how that was verified — usually a
`--test-*` suite, and for the audio work a Mesen capture of the hardware.

These documents deliberately do not name an asset-pack version number.  The
current one is `ALLSTAR_ASSET_VERSION` in `include/allstar_asset_pack.h`, and
the loader rejects anything else -- five documents drifted to stale version
numbers before that rule was adopted.

`docs/GHIDRA_COVERAGE.md` holds the authoritative coverage figure. The
documents here each quote the running total at the time they were written, so
prefer that file for the current number.

## Boot, frame and system

| document | what it covers |
|---|---|
| [KERNEL.md](KERNEL.md) | the `$0000..$005F` vectors, `rst $08`/`rst $10` dispatch, OAM DMA, the interrupt mask, `$0A91`'s one-player-only RNG reset |
| [BOOT.md](BOOT.md) | `$0150`/`$0156` wipes and the preserved seed, the `$02AC` title selector, the `$0324`/`$035F` handshake, attract, `$0214`'s session |
| [FRAME_SPINE.md](FRAME_SPINE.md) | `$2729`'s vblank handler and role-3 stall, `$276D`'s role-ordered body, `$279E`'s link substitution, the copyright screen, `$1699`'s flashing banner |
| [MODE_ROUTING.md](MODE_ROUTING.md) | the `$0267` mode table and how the five game modes are reached |
| [JOYPAD.md](JOYPAD.md) | `$2639` — the authority on the button packing every mask in the port uses |
| [LINK_CABLE.md](LINK_CABLE.md) | `$267F`, `$2FD0`, the role byte, and the outgoing path from `$C16E` |
| [MENU_AND_AUDIO.md](MENU_AND_AUDIO.md) | `$038F`'s menu, `$32B8`/`$32E9` voice state, and the known data-as-code misclassifications |
| [SETTINGS.md](SETTINGS.md) | `$231E`'s settings screen and the four mode-indexed dispatchers |
| [SCREEN_ART.md](SCREEN_ART.md) | `$04B1`'s eight screens, `$0271`'s copyright pair, and the `$2D4F` portraits and logos |
| [CAPTIONS.md](CAPTIONS.md) | `$07E3`'s caption script — 25 layouts covering every prompt in the game |

## Audio

| document | what it covers |
|---|---|
| [TITLE_MUSIC.md](TITLE_MUSIC.md) | `$029C`'s song `$01`, and `$35B6`'s NR51 routing that puts the two square voices on opposite sides |
| [SFX_ENVELOPE.md](SFX_ENVELOPE.md) | `$2AB5`/`$2F88`/`$2FB0` and the NR12 envelope every square cue decays through |
| [APU_PROGRAM.md](APU_PROGRAM.md) | `$35B6`'s per-voice programmer and the wave-table cache |
| [ONE_ON_ONE_PRESENTATION_AUDIO.md](ONE_ON_ONE_PRESENTATION_AUDIO.md) | the gameplay cues and the presentation they belong to |

## Game modes

| document | what it covers |
|---|---|
| [TOURNAMENT.md](TOURNAMENT.md) | mode `$04` end to end — the `$0F2E` driver, the bracket, the postgame spine, the bank-2 entrant selector |
| [ACCURACY.md](ACCURACY.md) | the one-player shootout route and its HUD writers |
| [FREE_THROW.md](FREE_THROW.md) | the `$0C8E` route, attempts, aim, and the mode-specific art |
| [HORSE.md](HORSE.md) | the `$0CDF`/`$0D57` caller/matcher loop, letters, and the shared court |
| [SHOT_RESULT.md](SHOT_RESULT.md) | `$1AF9`'s result table and the shared shot outcome path |

## One-on-One

Shared with the other modes wherever the cartridge shares it.

| document | what it covers |
|---|---|
| [ONE_ON_ONE_LIFECYCLE.md](ONE_ON_ONE_LIFECYCLE.md) | match start, clock, possession, overtime, exit |
| [ONE_ON_ONE_LIVE_FLOW.md](ONE_ON_ONE_LIVE_FLOW.md) | the live per-frame order of play |
| [ONE_ON_ONE_CONTROLLERS.md](ONE_ON_ONE_CONTROLLERS.md) | `$702D`'s human controller and its input paths |
| [ONE_ON_ONE_AI.md](ONE_ON_ONE_AI.md) | the CPU's decision state |
| [CPU_TARGET.md](CPU_TARGET.md) | `$7182`/`$7190` steering, the difficulty tables, `$73C9`'s head and the two route tables |
| [ONE_ON_ONE_ANIMATION.md](ONE_ON_ONE_ANIMATION.md) | `$6A8C`'s record machine and `$782E`'s movement selection |
| [DEFENSE_JUMP.md](DEFENSE_JUMP.md) | `$6BF9`/`$6C27`/`$6C4D` — the lift that makes a defensive jump visible |
| [ONE_ON_ONE_DEFENSE.md](ONE_ON_ONE_DEFENSE.md) | steals, contests, and post-contact recovery |
| [ONE_ON_ONE_PLAYER_COLLISION.md](ONE_ON_ONE_PLAYER_COLLISION.md) | `$6E3C`'s directional player-pair collision |
| [ONE_ON_ONE_POSSESSION.md](ONE_ON_ONE_POSSESSION.md) | who owns the ball and how that changes |
| [ONE_ON_ONE_SHOOTING.md](ONE_ON_ONE_SHOOTING.md) | gather, release, flight and rim contact |
| [ONE_ON_ONE_RNG.md](ONE_ON_ONE_RNG.md) | the `$0714`/`$072F` generator and its consumers |
| [ONE_ON_ONE_ASSETS.md](ONE_ON_ONE_ASSETS.md) | the art the mode loads and where it comes from |
| [ONE_ON_ONE_GHIDRA_PATH.md](ONE_ON_ONE_GHIDRA_PATH.md) | the original Ghidra route through the mode |

## Machine-readable manifests

The `*_COVERAGE.json` files beside these documents are the checked denominators.
`tools/check_*.py` gates them; run them all with the commands in the top-level
`README.md`.
