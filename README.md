# NBA All-Star Challenge — Native C Port

Work-in-progress native C99 port of **NBA All-Star Challenge** for Game Boy (Beam Software / LJN).

The project is a native reimplementation, not an emulator wrapper. The current build has a working Win32 shell, software renderer, scene prototypes, roster presentation, basic projectile physics, simple CPU behavior, and PCM playback. It is **not yet a gameplay-complete or frame-accurate port**.

## Current Status

| Area | Status | Notes |
|---|---|---|
| Win32 runtime and 160×144 framebuffer | Implemented | Builds and runs natively. |
| Intro, menu, settings, and roster screens | Settings verified | ROM defaults and cycles persist for the session and feed the relevant native modes; screen-art parity remains partial. |
| One-on-One | 100% of both fixed manifests | All 50 gameplay requirements and all 50 RNG/animation-asset/contact requirements are verified. This is Ghidra/manual-grounded mode coverage, not a claim of frame-perfect timing. |
| Free Throws | Prototype | Gauge and ball flight exist, but scoring parameters and completion flow need correction. |
| H-O-R-S-E | Prototype | Does not yet implement called-shot matching or a complete turn/win loop. |
| Accuracy/three-point scene | Prototype | Correctly routed and consumes time/position settings, but remains a simplified unverified five-position contest. |
| Tournament | Gameplay flow verified | Four quarterfinals, two semifinals, the final, champion state, and title return are covered by deterministic tests. |
| Two-player gameplay | Not implemented | The title-screen choice is visual state only; there is one native input stream. |
| Audio | Partial project-wide | Win32 PCM mixer works; focused One-on-One `$04/$05/$09/$0C/$0D/$0E/$0F` programs are decoded from the ROM's `$3014` data and synthesized from recovered square/sweep/noise state. The complete music/APU sequencer is not ported. |
| ROM asset pack | Partial project-wide | Version 10 contains One-on-One court, animated net, shared player animation art, ball/shadow, all 24 `$6C60` lists, and seven focused decoded audio programs including the `$04` foul and `$09` rim cues; other screens, portraits, music, and remaining sound programs still need migration. |
| Ghidra-to-C routine coverage | 34/118 verified | All 118 reviewed bank-aware functions recover and decompile cleanly; 29 mappings remain candidates and 55 are unmapped. The newly reviewed `$21FA` roster-palette routine is fully verified. |
| Verified project milestones | 40.00% | 10 of 25 strict milestones; analysis is 6/7 and gameplay parity is 4/11. |
| Scoped One-on-One parity | 100.00% | 50 of 50 Ghidra/manual-grounded gameplay requirements. |
| Remaining One-on-One focus | 100.00% | 50 of 50 fixed requirements: RNG 10/10, animation/assets 20/20, collision/reaction 20/20. Frame-perfect synchronization remains deliberately outside this denominator. |
| One-on-One shooting through inbound | 100.00% | 22 of 22 focused requirements. The recovered 258-state sequence is complete; the native presentation intentionally plays it at 3× speed for a roughly 1.43-second score-to-inbound transition. |
| One-on-One presentation/audio | 100.00% | 55 of 55 focused requirements: roster-indexed `$21FA` OBJ palettes over shared gameplay art, `$7138` hoop-facing shots, live `$2B14/$2B88` steals without score fade, charging/blocking presentation and command `$04`, animated net, seven decoded ROM cues, inbound/take-back, grounding, CPU route, defender recovery, and rim behavior. Whole-engine APU/music parity remains outside this denominator. |

See [docs/GHIDRA_COVERAGE.md](docs/GHIDRA_COVERAGE.md) for the audited coverage baseline and missing-work matrix.

Run the machine-readable coverage gate with:

```powershell
python tools\check_coverage.py
```

The focused One-on-One parity denominator is reported separately:

```powershell
python tools\check_one_on_one_coverage.py
python tools\check_one_on_one_remaining_coverage.py
python tools\check_one_on_one_presentation_audio_coverage.py
```

For future commit decisions, compare the working tree with the currently committed manifest:

```powershell
python tools\check_coverage.py --baseline-ref HEAD --require-delta 2
```

## ROM Facts

The verified USA/Europe ROM is:

- 64 KiB (`0x10000` bytes)
- MBC1 cartridge type (`0x01`)
- Four 16 KiB ROM banks
- No cartridge RAM

Banks 1–3 all execute in the Game Boy switchable CPU window at `$4000..$7FFF`. Ghidra must represent them as separate overlays or equivalent banked address spaces; treating the ROM as one flat CPU address space produces incorrect analysis.

## Building

### Windows with MSVC

```powershell
.\build.ps1
```

To build the executable and regenerate its local One-on-One asset pack in one
step, provide the user-owned ROM:

```powershell
.\build.ps1 -RomPath "path\to\NBA All-Star Challenge (USA, Europe).gb"
```

The build also checks `ALLSTAR_ROM_PATH` and then `build\nba_allstar.gb`. It
never commits or embeds the ROM or generated pack.

This produces:

- `build/allstar_port.exe` — CLI test harness, ROM validator, and asset-pack builder.
- `build/allstar_port_game.exe` — Win32 game executable.

The current MSVC build succeeds without warnings.

### Validate the ROM

```powershell
.\build\allstar_port.exe --rom-test "path\to\NBA All-Star Challenge (USA, Europe).gb"
```

### Build the current asset pack

```powershell
.\build\allstar_port.exe --build-assetpack "path\to\game.gb" build\allstar.assetpack
```

Version 10 extracts the One-on-One court tiles/map, the separate 17-tile score-net stream, three shared player action-family tile stores, 60 frame maps, ball/shadow tiles and descriptors, all 24 animation-control lists, and decoded command-`$04/$05/$09/$0C/$0D/$0E/$0F` square/noise programs from the ROM. `$2DD2->$21FA` proves that selected players reuse the shared gameplay body art and differ through exact roster-record OBJ palettes; there is no per-player gameplay body-sheet table to extract. Portraits, other-mode graphics, music, and the remaining sound programs are still outside the pack.

The Win32 game requires a valid version-10 `build\allstar.assetpack` and now
reports a clear error instead of silently replacing missing art with the
procedural test fallback.

### Run the game

```powershell
.\build\allstar_port_game.exe
```

## Verification

```powershell
.\build\allstar_port.exe --test-all
```

The tests cover roster invariants, exact 8.8 launch tables and vectors, `$1CED/$1E77` contacts, `$798B/$FFD6` 2/3-point regions, `$72EA/$74BB/$732C/$755D/$756C/$75CD` CPU behavior, `$71EE` contest limits, `$71B3/$762C` steal thresholds, `$0A78/$2B14` steal transfers, `$6A8C/$6C60` animation records, shot-gather `$6B72` movement, direct movement/contact latches, charging/blocking, defensive jumps and recovery locks, exact player/ball composition, routing, settings, lifecycle, `$702D` input timing, `$7F37` origins, `$077D` recovery, traveling, score-ball delayed gravity/bounces, tournament flow, and input-free scene ticking. Broader whole-game and frame-perfect parity remain unverified.

For manual comparison with the original ROM:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Automated Mesen traces cover shot input and movement during gather, complete run/shot record playback, `$2DD2->$21FA` selected-roster palettes, `$7138` shot facing on both sidelines, `$2B14/$2B88` steals, `$2CCA->$05A3->$0C49->$20F7` foul presentation, the CPU `$7170->$72EA->$732C->$755D->$756C` path, made-basket/net flow, focused `$04/$05/$09/$0C/$0D/$0E/$0F` APU programs, final `$6F2A` ball placement, rim/boundary recovery, defense transitions, exact RNG, contact rules, and extracted One-on-One graphics plus ball/shadow OAM composition. Broader whole-game WRAM snapshots and frame-difference tests remain to be implemented. See [the exact Ghidra-to-C path](docs/parity/ONE_ON_ONE_GHIDRA_PATH.md), [live-flow evidence](docs/parity/ONE_ON_ONE_LIVE_FLOW.md), [presentation/audio evidence](docs/parity/ONE_ON_ONE_PRESENTATION_AUDIO.md), [shooting evidence](docs/parity/ONE_ON_ONE_SHOOTING.md), [animation parity note](docs/parity/ONE_ON_ONE_ANIMATION.md), and [player-collision note](docs/parity/ONE_ON_ONE_PLAYER_COLLISION.md).

## Reverse Engineering

- `disassembly/` contains the four-bank `mgbdis` output. Labels generated by `mgbdis` are analysis candidates and may identify data as code.
- `tools/ghidra/` contains the bank-aware headless scripts, the reviewed function seed manifest, and the MCP bridge helper.
- `tools/decomp/ghidra_decompiled.c` is the generated preliminary Ghidra export; the validated machine-readable result is `build/ghidra_function_inventory.json`.

Read [PORTING.md](PORTING.md) for architecture and verification rules, and [AGENTS.md](AGENTS.md) for repository contribution requirements.
