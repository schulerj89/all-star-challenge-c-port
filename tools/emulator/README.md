# Headless emulator parity traces

`trace_one_on_one_input.lua` drives the original cartridge from boot into a
One-on-One match, chooses distinct default players, and checks the bank-1
`$702D` human shot-input paths in Mesen 2.

The script captures a gameplay savestate and checks both release sequences:

- A then A: the first A enters action `$0A`; the second new-A bit releases
  directly from shot phase `0`.
- A then B: held B sets phase `1` and `$C16A=1`; the following player update
  clears the latch, advances to phase `2`, and releases through `$7F0A` with
  zero planar velocity. The next `$6A8C:$6B34` record load must select the
  extracted dunk/drop display frame `$13`.

Run it with a valid user-supplied ROM (the ROM is never copied into the repo):

```powershell
$mesen = 'C:\path\to\Mesen.exe'
$script = (Resolve-Path '.\tools\emulator\trace_one_on_one_input.lua').Path
$rom = 'C:\path\to\NBA All-Star Challenge (USA, Europe).gb'
$arguments = "--testRunner --enableStdout --timeout=30 `"$script`" `"$rom`""
$process = Start-Process -FilePath $mesen -ArgumentList $arguments `
    -WindowStyle Hidden -Wait -PassThru
exit $process.ExitCode
```

Exit code `0` and the final `TRACE PASSED` line are required. The assertions
cover `$FFAE` new input, `$FFAF` held input, player action/shot phase,
possession `$FFCF`, release latch `$C16A`, zero planar velocity, negative
vertical velocity, and display frame `$13`.

`trace_one_on_one_defense.lua` follows the same boot/menu route, then injects
deterministic native contact states at the reviewed routine boundaries. It
confirms a `$2B14` steal transfer and proves that `$2B88` rejects `$2B6C` jump
contact while `$FFF8=1`, then accepts the same post-contact recovery after the
flag clears. Run it by substituting that filename for the script in the command
above; exit code `0` and `TRACE PASSED: $2B14 steal and $2B6C/$2B88 live-shot
lock` are required.

`trace_one_on_one_rng.lua` resolves the fixed-bank `$0714/$072F` generator
used by One-on-One. It asserts the first 32 gameplay-frame values of `$FFFB`,
records the writer PC and `$702D/$74BB/$75CD` consumers, and verifies that the
stream advances every other frame. Run it with the same command and require
exit code `0` plus `TRACE PASSED: $FFFB writer and 32 One-on-One frame values`.

`trace_one_on_one_animation.lua` follows bank 1 `$6A8C` under controlled
held-ball movement, no-ball movement, defensive jump, and steal inputs. It
asserts all four `$782E` direction branches in both possession families,
directional idle, `$70FD`'s middle-family jump, the following steal family,
and a six-frame record reload through player fields `+$00..+$04`. Require exit
code `0` and `TRACE PASSED: $782E/$6A8C directional actions and record cadence`.

`trace_one_on_one_player_collision.lua` follows bank 1
`$6A8C->$6B72->$6BAD->$6E3C/$6EC0/$6EEA`. It injects controlled player
coordinates during rightward input and verifies both the coordinate result and
the ROM's player `+$0C`/global `$C16B` contact latches. Require exit code `0`
and `TRACE PASSED: $6E3C directional player-pair collision`; this proves a
blocked move preserves X while moving away or vertical separation permits the
exact four-pixel move.

`trace_one_on_one_contact_rules.lua` drives fixed-bank
`$2C50->$2CCA->$0AC5` with exact possession-owner geometry. It proves charging
when the owner is the expiring offender, blocking when the defender is the
offender, and latch clearing for the protected shot-gather action `$0A`.
Require exit code `0` and `TRACE PASSED: $2C50/$2CCA/$0AC5 charging, blocking,
and protected action`.

`trace_one_on_one_assets.lua` captures the decompressed player, net, court,
and ball graphics from the original ROM and checks exact lengths and hashes.
It verifies final live `$6F2A` held-ball X/Y/Z against owner action, direction,
and record, then exercises `$6945->$69F5` across all three shadow-height bands.
Require exit code `0` and `TRACE PASSED: One-on-One assets, $6F2A held ball, and
$6945/$69F5 OAM`.

`trace_one_on_one_shot_results.lua` releases on the cartridge's recovered
make record and asserts the complete result path: `$6A8C` player `+$03/+$18`,
`$7F37` release height, `$7C58` class/vector/VZ, and fixed
`$1CED->$1E0E` at `X/Y/Z=$54/$5C/$38`. Require exit code `0` and
`TRACE PASSED: $6A8C/$7F37/$7C58/$1CED launched make`.

`trace_one_on_one_score_presentation.lua` continues the same deterministic
make through fixed-bank `$1E0E->$1F23/$2F88->$0C13->$2D08->$27C7/$27EA
->$20F7->$27CC` and the next bank-1 `$702D` update. It asserts sound command
`$05` and score commit at `+65` frames, the eight exact BGP writes, a 34-frame
fade-out, the final `$FFEB=1` counted update at `+254`, and playable inbound
at `+258` frames. Require exit code `0` and
`TRACE PASSED: $1E0E/$2F88/$0C13/$27C7/$20F7/$27CC score presentation`.

`trace_one_on_one_presentation_audio.lua` follows roster selection into a
deterministic made basket. It asserts navigation command `$0F`, accepted-player
command `$0E`, movement command `$0D`, record-six dribble command `$0C`, final
`$6F2A` ball placement, all four `$1ECC` net frames, command `$08` at `+20`,
and command `$05` at `+65`. Require exit code `0` and
`TRACE PASSED: roster audio, $6F2A dribble placement, and $1ECC score net`.

`trace_one_on_one_miss_take_back.lua` forces a rim miss and follows fixed
`$1CED->$1D8C/$1F4D->$2AE2->$2B88`, then bank-1 `$78E9/$796C`. It asserts
the outer-boundary velocity stop, changed-owner `$FFD1` recovery, CPU route,
inside-region persistence, and outside-region take-back clearance.

`trace_one_on_one_take_back_violation.lua` seeds the state written by bank-1
`$7C58` when a changed owner tries to shoot before clearing. It proves the
next fixed-bank update takes `$C178->$2C50->$067C`, dispatches `$05A3` command
`$04`, reaches `$20F7` at `+161`, and resumes at `+204` with possession
awarded to the opposite player.

`trace_one_on_one_rim_audio.lua` forces the exact `$53/$5E/$37` rim cell and
follows `$1CED->$1D8C->$1F5F->$2F88->$3014`. It asserts cooldown `$08`,
that `$1F5F` preserves initial-flight `$FFF8`, command `$09`, program `$0B`,
priority `$23`, and captures noise-channel
`NR41/42/43/44=$EB/$F2/$5A/$BF`. Fixed `$1E5B/$1E77`, covered by the physics
regressions, clears `$FFF8` later on the first ground bounce.

`trace_one_on_one_live_flow.lua` holds a direction through a complete run
cycle and through shot gather. It asserts all run display frames, continued
`$6A8C->$6B72` movement under action `$0A`, and release only on the second A.

`trace_one_on_one_cpu_decision.lua` awards the live ball to the CPU and traces
`$7170->$72EA->$74BB->$732C->$755D->$756C->$7C58`. It requires two target
stages and a nonzero, record-gated gather delay before release.
