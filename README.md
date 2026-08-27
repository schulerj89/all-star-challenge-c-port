# NBA All-Star Challenge — Native C Port

Work-in-progress native C99 port of **NBA All-Star Challenge** for Game Boy (Beam Software / LJN).

The project is a native reimplementation, not an emulator wrapper. The current build has a cross-platform SDL 3 host, software renderer, scene prototypes, roster presentation, native game logic, and PCM playback. The original Win32 host remains available as an optional reference during the transition. It is **not yet a gameplay-complete or frame-accurate port**.

## Current Status

| Area | Status | Notes |
|---|---|---|
| SDL runtime and 160×144 framebuffer | Implemented | Builds on macOS and iOS, and uses the same SDL host on Windows and Linux. |
| Intro, menu, settings, and roster screens | Settings verified | ROM defaults and cycles persist for the session and feed the relevant native modes; screen-art parity remains partial. |
| One-on-One | 100% of both fixed manifests | All 50 gameplay requirements and all 50 RNG/animation-asset/contact requirements are verified. This is Ghidra/manual-grounded mode coverage, not a claim of frame-perfect timing. |
| Free Throws | 32/32 gameplay/presentation | Bank-2 single-player selection without VS, `$0C8E/$100F` lifecycle and music stop, exact `$22A9/$1A25` moving reticle, 8.8 aim/launch, `$1A7E` rating-based clean makes, rim/net scoring, attempts/results, separate close-up assets, and `$1C1D` priority are native. |
| H-O-R-S-E | 30/30 gameplay/presentation | Two roster players, caller/matcher turns, exact 50-spot CPU table, saved X, letters, shared shot animation/physics, command `$07`, winner, and exit are native and Mesen-traced. |
| Accuracy Shootout | 25/25 one-player scope | Single-player selection, exact 50 positions/custom editor, marker approach, shared shot/net, exact `$76A7` court-panel HUD, scoring, timer/result, and `$02` audio are traced and playable. |
| Tournament | Gameplay flow verified | Four quarterfinals, two semifinals, the final, champion state, and title return are covered by deterministic tests. |
| Two-player gameplay | Not implemented | The title-screen choice is visual state only; there is one native input stream. |
| Audio | Partial project-wide | SDL and Win32 PCM output work; the ROM-derived four-channel title/menu song loops natively, and eleven focused programs include Accuracy `$02`, Horse `$07`, Free Throw `$08/$0A`, and One-on-One/selector cues. Other songs and the complete APU sequencer remain. |
| ROM asset pack | Partial project-wide | Version 17 contains One-on-One art, Free Throw art/maps, all 24 `$6C60` lists, shared Horse/Accuracy assets, eleven decoded audio programs, and the decoded title/menu song; portraits, other songs, and remaining sound programs still need migration. |
| Ghidra-to-C routine coverage | 67/145 verified | All 145 reviewed bank-aware functions recover/decompile; 38 mappings are candidates and 40 are unmapped. Accuracy's expanded eleven-routine scope is 8 verified/3 candidate. |
| Verified project milestones | 56.00% | 14 of 25 strict milestones; analysis is 6/7 and gameplay parity is 8/11. |
| Scoped Free Throw gameplay/presentation | 100.00% | 32 of 32 requirements, including exact mode-specific assets, result layout, prior-OAM priority, and the made-ball gravity hold. |
| Scoped H-O-R-S-E gameplay/presentation | 100.00% | 30 of 30 requirements; strict whole-shared-routine Ghidra coverage is 27/45 (60.00%). |
| Scoped One-on-One parity | 100.00% | 50 of 50 Ghidra/manual-grounded gameplay requirements. |
| Remaining One-on-One focus | 100.00% | 50 of 50 fixed requirements: RNG 10/10, animation/assets 20/20, collision/reaction 20/20. Frame-perfect synchronization remains deliberately outside this denominator. |
| One-on-One shooting through inbound | 100.00% | 22 of 22 focused requirements. The recovered 258-state sequence is complete; the native presentation intentionally plays it at 3× speed for a roughly 1.43-second score-to-inbound transition. |
| One-on-One presentation/audio | 100.00% | 60 of 60 focused requirements: roster-indexed `$21FA` OBJ palettes over shared gameplay art, `$7138` hoop-facing shots, `$702D/$6B34` dunk display, corrected held-ball rows, live `$2B14/$2B88` steals without score fade, charging/blocking/take-back presentation and command `$04`, score-ball/net priority, seven decoded ROM cues, grounding, CPU route, defender recovery, and rim behavior. Whole-engine APU/music parity remains outside this denominator. |

### ROM-wide coverage

The tables above are per-mode manifests. The measure of the cartridge itself is
separate: the ROM was rebuilt byte-exactly from `disassembly/bank_*.asm` and
walked by recursive descent from the vectors, which gives a routine inventory
that does not depend on what anyone had already noticed.

Against that inventory, **the port names every reachable instruction in the
cartridge**. Reachable code not named in `src/` or `include/` stands at **35
bytes**, and those 35 are the four `$7DD1` sub-tables, which are proven data
rather than code.

See [docs/GHIDRA_COVERAGE.md](docs/GHIDRA_COVERAGE.md) for the audited coverage
baseline, that result in full, and the missing-work matrix.
[docs/parity/](docs/parity/README.md) indexes one document per ROM subsystem.

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

### macOS with CMake and SDL 3

CMake fetches the pinned SDL 3 source and builds it statically, so a separate
SDL installation is not required.

```sh
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --parallel
build/macos/allstar_port --build-assetpack \
  "/path/to/NBA All-Star Challenge (USA, Europe).gb" \
  build/macos/allstar.assetpack
cmake -E copy_if_different build/macos/allstar.assetpack \
  build/macos/allstar_port_game.app/Contents/Resources/allstar.assetpack
open build/macos/allstar_port_game.app
```

The generated asset pack is local and ignored by Git; the ROM is never copied
or embedded. Linux and Windows can use the same CMake flow. On those platforms,
place `allstar.assetpack` beside the `allstar_port_game` executable.

### iPhone Simulator

The iOS build bundles a local generated asset pack into the app build and uses
landscape touch controls. It does not bundle the source ROM. Build the macOS
target and generate `build/macos/allstar.assetpack` first, then run:

```sh
cmake -S . -B build/ios-sim -G "Unix Makefiles" \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DALLSTAR_ASSETPACK_PATH="$(pwd)/build/macos/allstar.assetpack"
cmake --build build/ios-sim --parallel

xcrun simctl boot "iPhone 17 Pro" 2>/dev/null || true
open -a Simulator
xcrun simctl bootstatus "iPhone 17 Pro" -b
xcrun simctl install "iPhone 17 Pro" \
  build/ios-sim/allstar_port_game.app
xcrun simctl launch "iPhone 17 Pro" \
  com.schulerj89.allstarchallenge.ios
```

For a physical iPhone, generate an Xcode build with `iphoneos`, open the
generated project, select a Personal Team for signing, and run it on the
connected phone. The local asset pack remains inside the ignored build tree.

### Controls

| Game Boy input | Keyboard | Gamepad | iPhone touch |
|---|---|---|---|
| D-pad | Arrow keys | D-pad | Left D-pad |
| A | `Z` or `J` | South / bottom face button | Right `A` button |
| B | `X` or `K` | East / right face button | Right `B` button |
| Start | Return | Start | `START` pill |
| Select | Space or Shift | Back | `SELECT` pill |
| Cycle palette | `P` | — | Top-right `COLOR` button |

Press `1`, `2`, or `3` to select the original green, grayscale, or modern
palette. Press `P` or the iPhone `COLOR` button to cycle palettes.

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

Version 17 extracts the One-on-One court/player/ball/net data and separate Free Throw `$640F/$6EF1/$708E/$7F69` graphics, shooter/net/OBJ maps, all 24 animation-control lists, decoded command-`$02/$04/$05/$07/$08/$09/$0A/$0C/$0D/$0E/$0F` programs, and the original four-channel title/menu song. Horse and Accuracy reuse the One-on-One court assets and source tile 41 for their exact `$76` marker; `$02` is the Accuracy result cue. Portraits, other songs, and remaining sound programs are outside the pack.

The game requires a valid version-20 `allstar.assetpack` and
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

The tests cover roster invariants, exact 8.8 launch/contact physics, the complete `$7170-$761A` CPU and `$702D/$714D` player controllers, One-on-One, Free Throw through results/exit, Horse caller/matcher/spot/letter/winner rules, the tournament, the `$2729` frame spine, the `$0000-$005F` vectors, the `$07E3` caption script, and the audio work below. Broader whole-game and frame-perfect parity remain unverified.

Individual suites run under their own flag — `--test-title-music`,
`--test-sfx-envelope`, `--test-defense-jump`, `--test-frame`, `--test-kernel`
and the rest; `--help` lists them.

For manual comparison with the original ROM:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Automated Mesen traces cover One-on-One, Free Throw, and the Horse `$4000->$0CDF->$0D57->$7AFD->$0E26` path with exact X art and live `$07` APU writes. Broader whole-game WRAM snapshots and frame-difference tests remain. See [the Horse conversion](docs/parity/HORSE.md), [the Free Throw conversion](docs/parity/FREE_THROW.md), and [the controller conversion](docs/parity/ONE_ON_ONE_CONTROLLERS.md).

Two of those traces capture the cartridge's own APU register writes and are
diffed frame by frame against the port's decoded programs — `trace_title_music`
found the discarded NR51 stereo routing and `trace_navigation_sfx` the flat
square envelopes. Both bugs were in rendering; the decoded note data was exact
in each case. See [the title music](docs/parity/TITLE_MUSIC.md) and [the cue
envelopes](docs/parity/SFX_ENVELOPE.md), and `tools/emulator/README.md` for how
to run them.

## Reverse Engineering

- `disassembly/` contains the four-bank `mgbdis` output. Labels generated by `mgbdis` are analysis candidates and may identify data as code.
- `tools/ghidra/` contains the bank-aware headless scripts, the reviewed function seed manifest, and the MCP bridge helper.
- `tools/decomp/ghidra_decompiled.c` is the generated preliminary Ghidra export; the validated machine-readable result is `build/ghidra_function_inventory.json`.

Read [PORTING.md](PORTING.md) for architecture and verification rules, and [AGENTS.md](AGENTS.md) for repository contribution requirements.
