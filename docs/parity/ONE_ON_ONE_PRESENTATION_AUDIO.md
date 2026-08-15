# One-on-One presentation and audio evidence

Last reviewed: **2026-08-15**

## Result

The focused event/path metric is **20/20 (100.00%)**, up from the reconstructed
pre-audit state of **6/20 (30.00%)**. This metric covers exact ROM event timing,
animation selection, ball placement, and sound-command dispatch. It does not
claim cycle-accurate Game Boy APU waveform synthesis: the native port maps the
recovered commands to user WAV files or deterministic PCM fallbacks.

## Ghidra and live-ROM path

| User-visible behavior | Recovered ROM path | Native C path |
|---|---|---|
| Score effect starts | `$1E0E->$1F33` writes `$C129=4`, `$C168=$14` | `allstar_one_on_one_score_presentation_begin_1e0e` initializes the exact state clock. |
| Net bends | `$1ECC` at `+20` writes `$61,$62/$67,$68/$69,$6A` | `allstar_one_on_one_score_net_frame_1ecc` selects `BEND`; renderer replaces the same six cells. |
| Net reaches deepest frame | `$1ECC` at `+35` writes `$6B,$6C/$6D,$6E/$6F,$70` | The helper selects `DEEP`. |
| Net returns and rests | `$1ECC` at `+50` repeats bend; at `+65` restores `$61,$62/$63,$64/$65,$66` | The helper returns `BEND`, then `REST`. |
| Net impact sound | `$1F26->$2F88` selects command `$08` at `+20` | `ALLSTAR_ROM_SCORE_EVENT_NET_SOUND` plays `ALLSTAR_SFX_SWISH`. |
| Score chime | `$1F23/$1F06->$2F88` selects command `$05` at `+65` | Score commit plays the separate multi-step `ALLSTAR_SFX_SCORE_CHIME`. |
| Net graphics source | `$1FFA->$2021` and `$2219` decompress bank 3 `$793F` to VRAM `$9600` | Asset-pack v5 stores 17 decoded tiles for signed BG IDs `$60..$70`. |
| One-on-One court source | `$0B9A->$04B1(A=1)->$050F` selects bank 3 `$7A23->$9000` and `$7E48->$9800` | The builder stores 86 court tiles and the 640-byte, 32-stride map. `$2243` is not credited because it belongs to another mode. |
| Final held/dribbling ball | Per update, `$7F37` runs first and `$6F2A` then overrides actions `$01/$04/$08/$0B/$10/$13`; `$6FEA` supplies record-indexed height | `allstar_renderer_rom_dribble_ball_6f2a` is the final live placement. `$7F37` remains the shot/gather origin path. |
| Dribble sound | `$6F2A->$6FE5` sends command `$0C` every update while player `+$03=6` | The live scene emits `ALLSTAR_SFX_DRIBBLE` on that exact record cadence. |
| Shoe squeak | `$782E` action change continues through `$78DD->$78E0` command `$0D` | Movement-action changes emit `ALLSTAR_SFX_SHOE_SQUEAK`. |
| Roster navigation | Bank 2 `$4113->$2AB5/$2F88` sends command `$0E` | Cursor navigation uses `ALLSTAR_SFX_MENU_MOVE`. |
| Accepted roster player | Bank 2 `$40F4->$411D->$2AB5/$2F88` sends command `$0F` | Both accepted player confirmations use the existing multi-step `ALLSTAR_SFX_MENU_SELECT` cue. |

The Mesen trace records the `$1ECC` phases at exact deltas `20/35/50/65`,
commands `$08/$05`, six consecutive command-`$0C` updates at animation record
six, command `$0D` on movement-action changes, and roster commands `$0E/$0F`.
It also asserts the live `$6F2A` result from the owner action, record, direction
bit, coordinates, and `$6FEA` height table.

## Native pacing

The pure state helper still traverses all 258 cartridge presentation states.
The native scene now consumes them at an explicit **3x presentation rate**,
so score contact to playable inbound takes about **1.43 seconds** rather than
the cartridge's roughly 4.3 seconds. This is a documented usability override;
net and audio events still occur at their exact underlying ROM-state offsets.

## Verification

```powershell
.\build.ps1 -RomPath "<user ROM>"
.\build\allstar_port.exe --test-all
python tools\check_one_on_one_presentation_audio_coverage.py
```

Live-ROM evidence:

- `tools/emulator/trace_one_on_one_presentation_audio.lua`
- `tools/emulator/trace_one_on_one_assets.lua`

The screenshot dump includes normal gameplay, `$6F2A` dribbling, shot lift,
flight, score/fade/inbound, and the extracted bend/deep/rest net frames.
