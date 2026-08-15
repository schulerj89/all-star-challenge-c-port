# One-on-One RNG Evidence

## Verified scope

The native One-on-One CPU no longer calls the host C library `rand()`.
Ghidra and the headless Mesen trace establish one shared ROM random byte at
`$FFFB`; target selection, shot decisions, and steal decisions observe that
same byte during a gameplay update.

| ROM evidence | Recovered behavior | Native counterpart |
|---|---|---|
| Fixed `$0B68->$0714` | Update RNG after the gameplay controller. `$FF8B` bit 0 produces the gameplay-visible every-other-frame cadence. | `allstar_game_tick` calls `allstar_rom_rng_end_frame_0714` after the One-on-One scene update. |
| Fixed `$072F` | Compute `seed*9+$002B`; add `$C133` and `$C0B6` to the low byte without carrying into the high byte; store at `$FFFB/$FFFC`. | `allstar_rom_rng_step_072f`. |
| `$C133`, `$C0B6` | P1 low BCD score and low BCD clock-seconds bytes perturb the low byte. | The game converts native score and clock seconds to packed BCD before the frame update. |
| Bank 1 `$702D/$74BB/$75CD` | Gameplay branches read `$FFFB`; reads do not advance it. | `allstar_ai_update` receives one `rom_random_byte` and reuses it for target, shot, and steal decisions. |

## Deterministic verification

`tools/emulator/trace_one_on_one_rng.lua` drives the cartridge to One-on-One,
records writes to `$FFFB`, records reviewed consumers, and asserts 32 frame-end
values. With zero score and `00` clock seconds, the low-byte stream holds each
value for two frames and follows:

```text
18, 18, 03, 03, 46, 46, A1, A1, D4, D4, 9F, 9F, ...
```

The native `--test-one-on-one-shooting` suite checks the same sequence, the
full 16-bit step, the no-carry entropy behavior, and BCD conversion. `--test-all`
includes this test.

The ROM seed's high byte can vary with boot/emulator state, but reviewed
One-on-One consumers read only `$FFFB`. The low byte at gameplay entry is
`$18` on the scripted route and is the synchronized native starting value.
