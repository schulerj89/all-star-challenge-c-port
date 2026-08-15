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
| Audio | Partial | Win32 PCM mixer works, but only a subset of events have samples and the ROM music sequencer is not ported. |
| ROM asset pack | Partial project-wide | Version 4 extracts the complete One-on-One court, player frames/tiles, ball/shadow tiles, and all 24 `$6C60` animation-control lists; other screens, roster records, and audio still need migration. |
| Ghidra-to-C routine coverage | 27/114 verified | All 114 reviewed bank-aware functions recover and decompile cleanly; 31 mappings remain candidates and 56 are unmapped. |
| Verified project milestones | 40.00% | 10 of 25 strict milestones; analysis is 6/7 and gameplay parity is 4/11. |
| Scoped One-on-One parity | 100.00% | 50 of 50 Ghidra/manual-grounded gameplay requirements. |
| Remaining One-on-One focus | 100.00% | 50 of 50 fixed requirements: RNG 10/10, animation/assets 20/20, collision/reaction 20/20. Frame-perfect synchronization remains deliberately outside this denominator. |
| One-on-One shooting through inbound | 100.00% | 22 of 22 focused requirements. The recovered 258-frame state sequence is complete; the native presentation intentionally plays it at 2× speed for a roughly 2.15-second score-to-inbound transition. |

See [docs/GHIDRA_COVERAGE.md](docs/GHIDRA_COVERAGE.md) for the audited coverage baseline and missing-work matrix.

Run the machine-readable coverage gate with:

```powershell
python tools\check_coverage.py
```

The focused One-on-One parity denominator is reported separately:

```powershell
python tools\check_one_on_one_coverage.py
python tools\check_one_on_one_remaining_coverage.py
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

The current build succeeds, although MSVC reports `fopen` deprecation warnings that still need cleanup.

### Validate the ROM

```powershell
.\build\allstar_port.exe --rom-test "path\to\NBA All-Star Challenge (USA, Europe).gb"
```

### Build the current asset pack

```powershell
.\build\allstar_port.exe --build-assetpack "path\to\game.gb" build\allstar.assetpack
```

Version 4 extracts the One-on-One court tiles/map, three player tile families, 60 frame maps, ball/shadow tiles and descriptors, and all 24 animation-control lists. It does not yet extract portraits, other-mode graphics, roster records, or audio sequences from the ROM.

The Win32 game requires a valid version-4 `build\allstar.assetpack` and now
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

The tests cover roster invariants, exact 8.8 launch tables and vectors, `$1CED/$1E77` contacts, `$798B/$FFD6` 2/3-point regions, `$72EA/$74BB/$756C/$75CD` CPU behavior, `$71EE` contest limits, `$71B3/$762C` steal thresholds, `$0A78/$2B14` steal transfers, `$6A8C/$6C60` animation records, `$6B72/$6E3C` direct movement and contact latches, `$2C50/$2CCA/$0AC5` charging/blocking, `$6C4D/$2B6C/$2B88` defensive jumps and recovery locks, exact player/ball composition, routing, settings, lifecycle, `$702D` input timing, `$7F37` origins, `$077D` recovery, traveling, tournament flow, and input-free scene ticking. Broader whole-game and frame-perfect parity remain unverified.

For manual comparison with the original ROM:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Automated Mesen traces cover shot input, the complete 258-frame made-basket sound/fade/playable-inbound sequence, defense transitions, exact RNG, the complete `$782E/$6A8C` directional action state, `$6B72/$6E3C` movement blocking, `$2C50/$2CCA/$0AC5` charging/blocking, and the extracted One-on-One graphics plus `$6945/$69F5` ball/shadow OAM composition. Broader whole-game WRAM snapshots and frame-difference tests remain to be implemented. See [the exact Ghidra-to-C path](docs/parity/ONE_ON_ONE_GHIDRA_PATH.md), [shooting evidence](docs/parity/ONE_ON_ONE_SHOOTING.md), [animation parity note](docs/parity/ONE_ON_ONE_ANIMATION.md), and [player-collision note](docs/parity/ONE_ON_ONE_PLAYER_COLLISION.md).

## Reverse Engineering

- `disassembly/` contains the four-bank `mgbdis` output. Labels generated by `mgbdis` are analysis candidates and may identify data as code.
- `tools/ghidra/` contains the bank-aware headless scripts, the reviewed function seed manifest, and the MCP bridge helper.
- `tools/decomp/ghidra_decompiled.c` is the generated preliminary Ghidra export; the validated machine-readable result is `build/ghidra_function_inventory.json`.

Read [PORTING.md](PORTING.md) for architecture and verification rules, and [AGENTS.md](AGENTS.md) for repository contribution requirements.
