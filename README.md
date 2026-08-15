# NBA All-Star Challenge — Native C Port

Work-in-progress native C99 port of **NBA All-Star Challenge** for Game Boy (Beam Software / LJN).

The project is a native reimplementation, not an emulator wrapper. The current build has a working Win32 shell, software renderer, scene prototypes, roster presentation, basic projectile physics, simple CPU behavior, and PCM playback. It is **not yet a gameplay-complete or frame-accurate port**.

## Current Status

| Area | Status | Notes |
|---|---|---|
| Win32 runtime and 160×144 framebuffer | Implemented | Builds and runs natively. |
| Intro, menu, settings, and roster screens | Settings verified | ROM defaults and cycles persist for the session and feed the relevant native modes; screen-art parity remains partial. |
| One-on-One | 100% of scoped manifest | All 50 fixed requirements are verified, including steals, CPU steal thresholds, defensive jumps, the `$FFF8` live-shot lock, and post-contact jump recovery. This is scoped behavioral coverage, not whole-mode frame/presentation parity. |
| Free Throws | Prototype | Gauge and ball flight exist, but scoring parameters and completion flow need correction. |
| H-O-R-S-E | Prototype | Does not yet implement called-shot matching or a complete turn/win loop. |
| Accuracy/three-point scene | Prototype | Correctly routed and consumes time/position settings, but remains a simplified unverified five-position contest. |
| Tournament | Gameplay flow verified | Four quarterfinals, two semifinals, the final, champion state, and title return are covered by deterministic tests. |
| Two-player gameplay | Not implemented | The title-screen choice is visual state only; there is one native input stream. |
| Audio | Partial | Win32 PCM mixer works, but only a subset of events have samples and the ROM music sequencer is not ported. |
| ROM asset pack | Partial | Basic 2bpp decoding works; most runtime art and roster data still come from compiled C tables. |
| Ghidra-to-C routine coverage | 6/97 verified | 97 reviewed bank-aware functions recover and decompile cleanly; `$077D`, `$0A78`, `$1F4D`, `$28E1`, `$2B14`, and `$2B6C` are narrowly verified, 25 mappings remain candidates, and 66 are unmapped. |
| Verified project milestones | 40.00% | 10 of 25 strict milestones; analysis is 6/7 and gameplay parity is 4/11. |
| Scoped One-on-One parity | 100.00% | 50 of 50 Ghidra/manual-grounded gameplay requirements. Broader collision reactions, presentation, assets, and frame parity remain outside this focused denominator. |

See [docs/GHIDRA_COVERAGE.md](docs/GHIDRA_COVERAGE.md) for the audited coverage baseline and missing-work matrix.

Run the machine-readable coverage gate with:

```powershell
python tools\check_coverage.py
```

The focused One-on-One parity denominator is reported separately:

```powershell
python tools\check_one_on_one_coverage.py
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

This command currently extracts a fixed tile range and packages the hardcoded roster. It does not yet extract all tilemaps, portraits, animation tables, roster records, or audio sequences from the ROM.

### Run the game

```powershell
.\build\allstar_port_game.exe
```

## Verification

```powershell
.\build\allstar_port.exe --test-all
```

The tests cover roster invariants, exact 8.8 launch tables and vectors, `$1CED/$1E77` contacts, `$798B/$FFD6` 2/3-point regions, `$72EA/$74BB/$756C` CPU targets and shot decisions, `$71EE` contest limits, `$71B3/$762C` steal thresholds, `$0A78/$2B14` steal transfers, `$6C4D/$2B6C/$2B88` defensive jumps and recovery locks, court limits, routing, settings, lifecycle, `$702D` input timing, `$7F37` origins, `$077D` recovery, traveling, tournament flow, and input-free scene ticking. Visuals and broader emulator/native frame parity remain unverified.

For manual comparison with the original ROM:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Automated Mesen traces now cover shot input and defense state transitions. Broader WRAM snapshots and frame-difference tests remain to be implemented.

## Reverse Engineering

- `disassembly/` contains the four-bank `mgbdis` output. Labels generated by `mgbdis` are analysis candidates and may identify data as code.
- `tools/ghidra/` contains the bank-aware headless scripts, the reviewed function seed manifest, and the MCP bridge helper.
- `tools/decomp/ghidra_decompiled.c` is the generated preliminary Ghidra export; the validated machine-readable result is `build/ghidra_function_inventory.json`.

Read [PORTING.md](PORTING.md) for architecture and verification rules, and [AGENTS.md](AGENTS.md) for repository contribution requirements.
