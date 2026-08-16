# Accuracy Shootout Parity Evidence

Last verified: **2026-08-16**

## Result

The one-player Accuracy Shootout scope is **25/25 (100.00%)**. This replaces
the invented five-rack/money-ball scene; the cartridge contains no racks,
money balls, or timing meter in this mode.

## Ghidra path followed

| ROM path | Recovered behavior | Native C |
|---|---|---|
| bank 2 `$4000:$4014-$401D->$4034` | Mode `$03` accepts P1 and skips opponent/VS | `allstar_game_mode_requires_opponent`, `roster_select_update` |
| fixed `$0E51` | Clears music, initializes court/player, timer and mode loop | `accuracy_init`, `tick_fixed` |
| bank 1 `$6C9B->$6CA2/$6CAB->$6DB7` | Ten positions per RNG-selected group; five exact 10-pair groups | `allstar_accuracy_init_0e51_6c9b`, `allstar_accuracy_next_position_6ca2` |
| bank 1 `$6D57` | New-position marker, four-pixel movement, ten recorded pairs | `allstar_accuracy_move_custom_cursor_6d57`, `allstar_accuracy_record_custom_position_6d57` |
| bank 1 `$7AFD->$100F` | Blink tile `$76`; do not shoot until player center/ground reaches target | `draw_marker`, `tick_fixed` approach phase |
| bank 1 `$702D->$714D->$6A8C->$7C58` | Shared gather, movable jump, release animation and ball launch | `begin_gather`, `launch`, shared One-on-One helpers |
| fixed `$0EE7->$0B20` | Increment attempts on every release | `allstar_accuracy_bcd_increment_0b20` |
| fixed `$1E0E->$0F1E->$6E1B` | Net/score presentation, increment makes at +80, reset for next target | `finish_attempt`, shared net/contact helpers |
| bank 1 `$76A7->$7739->$7749/$7765/$7790/$77A1->$780A` | Four-way HUD writer: active `MM:SS` at `$9821`, inactive `00:00` at `$982E`, active three-digit basketball score at `$9862`, inactive `000` at `$986F` | `allstar_accuracy_hud_76a7`, `draw_hud`; exact court tile coordinates `(1,1)`, `(14,1)`, `(2,3)`, `(15,3)` |
| fixed `$0FDE->$2F9E` | `TIME'S UP`, command `$02`, 240-frame result hold | Accuracy result phase and `ALLSTAR_SFX_ACCURACY_RESULT` |

## Emulator proof

`tools/emulator/trace_accuracy.lua` boots the original ROM through menu mode
`$03`, settings, and the selector. Its passing trace proves one `$4000` and
one `$4034` visit with `$FF91=1`, first target `$0C,$94`, three releases,
three attempts, one make, and four position calls. The observed make commits
at make +80 frames. It also executes all four indirect HUD writers and checks
the original tile map after a miss and a make: `01:58`/`000`, then
`01:49`/`002`, with the inactive panel held at `00:00`/`000`. On expiry,
command `$02` produces:

- square 1: `NR10/11/12/13/14 = 88/00/FF/5B/BE`;
- square 2: `NR21/22/23/24 = 3F/6F/41/BE`.

The asset pipeline follows `$2FB0 + $02*2 -> program $08 -> $3849 + $08*2`
and decodes streams `$3EC0/$3EC4`. The exported WAV contains 144 ROM frames,
matching the `$0D -> $90` duration entry.

## Verification

```powershell
.\build\allstar_port.exe --test-accuracy
python tools/check_accuracy_coverage.py --require-min 100
.\build\allstar_port.exe --export-accuracy-sfx build\allstar.assetpack build\proof\accuracy\accuracy_result_command_02.wav
```

The scoped 100% figure covers one-player Accuracy behavior and its reused
court/player/ball/net assets. It does not claim the whole `$3014` audio engine,
unreviewed two-human behavior, or frame-perfect rendering.
