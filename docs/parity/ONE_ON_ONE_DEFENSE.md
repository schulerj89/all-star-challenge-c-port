# One-on-One Defense Evidence

Last reviewed: **2026-08-15**

## Verified behavior

The port now reproduces the reviewed One-on-One steal, contest-jump, and
airborne rebound paths. The original cartridge does **not** contain a separate
goaltending violation or a live-shot block/possession branch in this path.

| ROM control flow | Recovered behavior | Native counterpart |
|---|---|---|
| `$702D->$2B14` | While the player does not own the ball, held B starts action `$07/$0F/$17` once per latch. | The human and CPU defense paths use the same steal attempt/latch state. |
| `$0A78` | Actions `$03/$0A/$12/$05/$0C/$14/$0E/$16` are protected from a steal. | `allstar_one_on_one_rom_action_eligible_0a78` contains the exact protected set. |
| `$2B14->$077D->$2B88` | A steal requires active opposing possession, a vulnerable owner, strict `|dx|<12` and `|dy|<8` ball contact, and opposing direction masks on either axis. | `allstar_one_on_one_rom_steal_contact_2b14` applies the same gates before the live scene transfers possession. |
| `$71B3`, table `$762C` | On held-ball contact, skill levels 1/2/3 press B when random byte `<$04/$19/$46`. | `allstar_ai_rom_should_steal_71b3` supplies the exact thresholds at the existing skill cadence. |
| `$702D->$70FD->$6C90`, table `$6C4D` | New A while defending selects jump action `$05/$0C/$14`; twelve six-frame records produce cumulative heights `0,9,16,21,24,26,26,24,19,12,4,0`. Because protected jump actions branch at `$709A`, player direction `+$07` remains the value sampled on the jump edge. | Human contest input and CPU `$71EE` contests start the same 72-frame defensive jump and retain that sampled movement direction. |
| `$6A8C->$6BF9->$6B72` | Each newly loaded jump record applies `+$07` through the ordinary four-pixel movement callbacks while the jump-height table controls visual Y. | Blocking no longer forces the native direction to zero; a moving block continues at the ROM's six-frame record cadence. |
| `$70BF->$2B6C` | A jumping player needs `$077D` planar contact and ball height strictly above `reach-8` and at or below `reach`. | `allstar_one_on_one_rom_jump_recovery_2b6c` preserves the strict eight-pixel band. |
| `$2B6C->$2B88` | `$FFF8=1` rejects possession during the shot's initial flight. `$1F5F` rim contact preserves `$FFF8`; `$1E5B/$1E77` clears it at the first ground bounce, after which the identical jump contact can recover the ball. `$2B88` changes ownership globals but does not write player action, `+$07`, `+$10`, or facing. | Post-ground-contact jump catches remain possible, and the native transfer preserves the current animation plus both direction fields and facing so movement continues after the catch. |
| `$70FD->$6A8C` landing | Jump families `$05/$0C/$14` contain twelve six-frame records followed by action transitions to `$06/$0D/$15`. The ROM does not cancel the pose early on rim contact. | The scene preserves all 72 frames, moves through their record boundaries, then returns to the normal held/free movement family without a post-block pause. |

## Emulator and native verification

`tools/emulator/trace_one_on_one_defense.lua` drives the original cartridge
into One-on-One and injects deterministic native contact states. It proves:

- `$2B14` transfers owner 2 to owner 1 when vulnerability, collision, and
  opposing-facing gates pass;
- `$2B88` leaves owner zero for the same jump contact while `$FFF8=1`;
- the first `$1E5B/$1E77` ground bounce clears `$FFF8` and allows the post-contact `$2B6C` jump recovery;
- `$2B88` preserves the rebounder's action `$05`, direction `+$07`, and stored direction `+$10`;
- the animation trace proves `$70FD` retains Right in `+$07` and `$6BF9/$6B72` advances player X during the jump.

Run the cartridge trace and native tests:

```powershell
$mesen = 'C:\path\to\Mesen.exe'
$script = (Resolve-Path '.\tools\emulator\trace_one_on_one_defense.lua').Path
$rom = 'C:\path\to\NBA All-Star Challenge (USA, Europe).gb'
$arguments = "--testRunner --enableStdout --timeout=30 `"$script`" `"$rom`""
Start-Process $mesen -ArgumentList $arguments -Wait -NoNewWindow

.\build\allstar_port.exe --test-one-on-one-shooting
```

The Ghidra inventory records `$0A78`, `$2B14`, and `$2B6C` as verified narrow
mappings. `$702D` and `$7170` remain candidate whole-routine mappings because
they also contain unrelated and other-mode branches.
