# NBA All-Star Challenge — Native C Port

Work-in-progress native C99 port of **NBA All-Star Challenge** for Game Boy (Beam Software / LJN).

The project is a native reimplementation, not an emulator wrapper. The current build has a working Win32 shell, software renderer, scene prototypes, roster presentation, basic projectile physics, simple CPU behavior, and PCM playback. It is **not yet a gameplay-complete or frame-accurate port**.

## Current Status

| Area | Status | Notes |
|---|---|---|
| Win32 runtime and 160×144 framebuffer | Implemented | Builds and runs natively. |
| Intro, menu, settings, and roster screens | Settings verified | ROM defaults and cycles persist for the session and feed the relevant native modes; screen-art parity remains partial. |
| One-on-One | 100% of both fixed manifests | All 50 gameplay requirements and all 50 RNG/animation-asset/contact requirements are verified. This is Ghidra/manual-grounded mode coverage, not a claim of frame-perfect timing. |
| Free Throws | 32/32 gameplay/presentation | Bank-2 single-player selection without VS, `$0C8E/$100F` lifecycle and music stop, exact `$22A9/$1A25` moving reticle, 8.8 aim/launch, `$1A7E` rating-based clean makes, rim/net scoring, attempts/results, separate close-up assets, and `$1C1D` priority are native. |
| H-O-R-S-E | 30/30 gameplay/presentation | Two roster players, caller/matcher turns, exact 50-spot CPU table, saved X, letters, shared shot animation/physics, command `$07`, winner, and exit are native and Mesen-traced. |
| Accuracy Shootout | 25/25 one-player scope | Single-player selection, exact 50 positions/custom editor, marker approach, shared shot/net, exact `$76A7` court-panel HUD, scoring, timer/result, and `$02` audio are traced and playable. |
| Tournament | Gameplay flow verified | Four quarterfinals, two semifinals, the final, champion state, and title return are covered by deterministic tests. |
| Two-player gameplay | Not implemented | The title-screen choice is visual state only; there is one native input stream. |
| Audio | Partial project-wide | Win32 PCM mixer works; eleven focused programs now include Accuracy `$02`, Horse `$07`, Free Throw `$08/$0A`, and One-on-One/selector cues. The complete music/APU sequencer is not ported. |
| ROM asset pack | Partial project-wide | Version 16 contains One-on-One art, Free Throw art/maps, all 24 `$6C60` lists, shared Horse/Accuracy assets, and eleven decoded audio programs; portraits, music, and remaining sound programs still need migration. |
| Ghidra-to-C routine coverage | 67/145 verified | All 145 reviewed bank-aware functions recover/decompile; 38 mappings are candidates and 40 are unmapped. Accuracy's expanded eleven-routine scope is 8 verified/3 candidate. |
| Verified project milestones | 56.00% | 14 of 25 strict milestones; analysis is 6/7 and gameplay parity is 8/11. |
| Scoped Free Throw gameplay/presentation | 100.00% | 32 of 32 requirements, including exact mode-specific assets, result layout, prior-OAM priority, and the made-ball gravity hold. |
| Scoped H-O-R-S-E gameplay/presentation | 100.00% | 30 of 30 requirements; strict whole-shared-routine Ghidra coverage is 27/45 (60.00%). |
| Scoped One-on-One parity | 100.00% | 50 of 50 Ghidra/manual-grounded gameplay requirements. |
| Remaining One-on-One focus | 100.00% | 50 of 50 fixed requirements: RNG 10/10, animation/assets 20/20, collision/reaction 20/20. Frame-perfect synchronization remains deliberately outside this denominator. |
| One-on-One shooting through inbound | 100.00% | 22 of 22 focused requirements. The recovered 258-state sequence is complete; the native presentation intentionally plays it at 3× speed for a roughly 1.43-second score-to-inbound transition. |
| One-on-One presentation/audio | 100.00% | 60 of 60 focused requirements: roster-indexed `$21FA` OBJ palettes over shared gameplay art, `$7138` hoop-facing shots, `$702D/$6B34` dunk display, corrected held-ball rows, live `$2B14/$2B88` steals without score fade, charging/blocking/take-back presentation and command `$04`, score-ball/net priority, seven decoded ROM cues, grounding, CPU route, defender recovery, and rim behavior. Whole-engine APU/music parity remains outside this denominator. |

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
python tools\check_free_throw_coverage.py
python tools\check_horse_coverage.py
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

To build the executable and regenerate its local gameplay asset pack in one
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

Version 16 extracts the One-on-One court/player/ball/net data and separate Free Throw `$640F/$6EF1/$708E/$7F69` graphics, shooter/net/OBJ maps, all 24 animation-control lists, and decoded command-`$02/$04/$05/$07/$08/$09/$0A/$0C/$0D/$0E/$0F` programs. Horse and Accuracy reuse the One-on-One court assets and source tile 41 for their exact `$76` marker; `$02` is the Accuracy result cue. Portraits, music, and remaining sound programs are outside the pack.

The Win32 game requires a valid version-15 `build\allstar.assetpack` and now
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

The tests cover roster invariants, exact 8.8 launch/contact physics, the complete `$7170-$761A` CPU and `$702D/$714D` player controllers, One-on-One, Free Throw through results/exit, and Horse caller/matcher/spot/letter/winner rules. Broader whole-game and frame-perfect parity remain unverified.

For manual comparison with the original ROM:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Automated Mesen traces cover One-on-One, Free Throw, and the Horse `$4000->$0CDF->$0D57->$7AFD->$0E26` path with exact X art and live `$07` APU writes. Broader whole-game WRAM snapshots and frame-difference tests remain. See [the Horse conversion](docs/parity/HORSE.md), [the Free Throw conversion](docs/parity/FREE_THROW.md), and [the controller conversion](docs/parity/ONE_ON_ONE_CONTROLLERS.md).

## Reverse Engineering

- `disassembly/` contains the four-bank `mgbdis` output. Labels generated by `mgbdis` are analysis candidates and may identify data as code.
- `tools/ghidra/` contains the bank-aware headless scripts, the reviewed function seed manifest, and the MCP bridge helper.
- `tools/decomp/ghidra_decompiled.c` is the generated preliminary Ghidra export; the validated machine-readable result is `build/ghidra_function_inventory.json`.

Read [PORTING.md](PORTING.md) for architecture and verification rules, and [AGENTS.md](AGENTS.md) for repository contribution requirements.
