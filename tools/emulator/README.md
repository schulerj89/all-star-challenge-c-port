# Headless emulator parity traces

`trace_one_on_one_input.lua` drives the original cartridge from boot into a
One-on-One match, chooses distinct default players, and checks the bank-1
`$702D` human shot-input paths in Mesen 2.

The script captures a gameplay savestate and checks both release sequences:

- A then A: the first A enters action `$0A`; the second new-A bit releases
  directly from shot phase `0`.
- A then B: held B sets phase `1` and `$C16A=1`; the following player update
  clears the latch, advances to phase `2`, and releases.

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
possession `$FFCF`, release latch `$C16A`, and nonzero ball vertical velocity.

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

`trace_one_on_one_assets.lua` captures the decompressed player, court, and
ball graphics from the original ROM and checks exact lengths and hashes. It
also verifies live `$7F37` held-ball X/Y/Z against the owning player frame,
then exercises `$6945->$69F5` across all three shadow-height bands. Require
exit code `0` and `TRACE PASSED: One-on-One assets, $7F37 held ball, and
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
