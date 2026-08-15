# NBA All-Star Challenge — Native C Porting Guide

This project is a native C reimplementation of **NBA All-Star Challenge** for Game Boy. The runtime must own game concepts—scenes, players, rules, AI, physics, rendering, and sound events—rather than execute Game Boy instructions or replay emulator logs.

This guide separates the intended architecture from the implementation that exists today. A C function with a similar name is not considered a verified port unless its state transitions and outputs have been compared with ROM behavior.

## 1. Porting Principles

1. **Native game concepts**
   - Runtime code should use native scene, player, ball, rule, animation, and audio structures.
   - MBC registers, VRAM addresses, OAM slots, and Game Boy I/O registers belong only in importers and reverse-engineering tools.

2. **User-supplied ROM asset pipeline**
   - The intended asset path is:

     ```powershell
     .\build\allstar_port.exe --build-assetpack <ROM_PATH.gb> build\allstar.assetpack
     ```

   - The current implementation only decodes a fixed range of 2bpp tiles and copies a hardcoded roster into the pack. Court/menu tilemaps, portraits, animations, roster records, and audio sequences are not yet fully extracted.
   - Several runtime screens and sprites currently use compiled derived-art headers. Moving these assets behind the user-supplied asset-pack boundary remains required.

3. **No decompilation dependency at runtime**
   - Ghidra pseudocode, assembly, symbols, and traces are reference evidence only.
   - The native executable must not require a decompilation checkout or emulator.

4. **Evidence-backed parity**
   - “Implemented” means the native behavior has a focused test or emulator comparison.
   - “Partial” means a native concept exists but important ROM behavior remains unmatched.
   - A symbol appearing in source code or comments is not coverage evidence.

## 2. Verified ROM Architecture

The checked USA/Europe ROM header reports the following:

| Component | Verified Game Boy target | Current native handling |
|---|---|---|
| CPU | Sharp SM83 at approximately 4.194 MHz | Native C; CPU timing is not emulated. |
| Cartridge | MBC1, cartridge type `0x01` | Importer reads the complete ROM as a file; no runtime mapper is needed. |
| ROM | 64 KiB, four 16 KiB banks | File loader accepts the full 64 KiB image. Some constants and analysis docs still need migration from the former 32 KiB assumption. |
| Cartridge RAM | None | No save-RAM behavior required. |
| Display | 160×144, four DMG shades | 32-bit software framebuffer with selectable palettes. |
| Tiles | 2bpp planar, 16 bytes per 8×8 tile | Basic decoder exists; ROM layout-specific extraction is incomplete. |
| Sprites | Up to 40 OAM entries | Rendered as native sprite/player concepts rather than hardware OAM. |
| Audio | Two pulse, wave, and noise channels | Current port uses PCM WAV mixing; ROM sequence interpretation is not implemented. |

### Bank mapping

| Physical ROM bank | File offsets | CPU address while selected |
|---|---|---|
| Bank 0 | `0x0000..0x3FFF` | `$0000..$3FFF` |
| Bank 1 | `0x4000..0x7FFF` | `$4000..$7FFF` |
| Bank 2 | `0x8000..0xBFFF` | `$4000..$7FFF` |
| Bank 3 | `0xC000..0xFFFF` | `$4000..$7FFF` |

Banks 2 and 3 must not be analyzed as CPU addresses `$8000..$FFFF`; those ranges are VRAM, external RAM, WRAM, echo RAM, OAM, and I/O in the Game Boy address space.

## 3. Reverse-Engineering Baseline

### Current Ghidra result

The headless pipeline now creates byte-verified `$4000..$7FFF` overlays for physical banks 1, 2, and 3, then recovers a conservative seed set rather than sweeping unknown data as code. The validated inventory contains 94 functions that all create and decompile successfully:

- 40 fixed-bank functions in bank 0;
- 46 functions in bank 1's coherent `$6945..$7FFF` gameplay region;
- 8 functions in bank 2's reviewed `$4000..$42A1` code region.

The generated C listing contains 107 functions when reset/interrupt vectors and thunks are included. Bank 3 has no reviewed function seeds yet: current references select it as an asset-copy source, but a complete code/data review is still required. Bank 1 `$76A7` is recorded as a deferred call-target candidate because unresolved flows make Ghidra build an invalid oversized body and time out.

`tools/ghidra/function_seeds.json` is the checked-in symbol/native-status source. The latest inventory records 70 functions as unmapped, 21 as candidate native analogues, and three as verified for narrowly tested behavior.

### Current `mgbdis` result

The four assembly files contain:

- 246 unique `Call_*` labels;
- 116 unique `Jump_*` labels;
- 251 unique direct `call` targets.

These figures are an analysis queue, not a function count. Without code/data maps, the disassembler sometimes interprets data as instructions. Function boundaries and meanings must be confirmed with cross-references, execution traces, and state effects.

### Required analysis workflow

1. Import bank 0 at `$0000..$3FFF`.
2. Import banks 1, 2, and 3 into separate overlays mapped to `$4000..$7FFF`.
3. Create functions at confirmed call targets and entry points.
4. Mark graphics, strings, pointer tables, animation tables, roster records, and audio data as data.
5. Name WRAM/HRAM fields from observed reads, writes, and emulator traces.
6. Record each native mapping with ROM address, input state, output state, evidence, and confidence.
7. Add a parity test before marking a mapping verified.

See [docs/GHIDRA_COVERAGE.md](docs/GHIDRA_COVERAGE.md) for the current audit.

Project progress is tracked by 25 strict milestones in `docs/COVERAGE_MANIFEST.json`. Only `verified` milestones receive credit. The current checkpoint is **10/25 (40.00%)**, up from the audited **3/25 (12.00%)** baseline. Analysis is **6/7 (85.71%)** and verified gameplay parity is **4/11 (36.36%)**.

One-on-One work also has a fixed 50-requirement scoped denominator in `docs/parity/ONE_ON_ONE_COVERAGE.json`. It is currently **30/50 (60.00%)** and can be checked with `python tools/check_one_on_one_coverage.py`. This focused metric does not add credit to the strict project manifest.

## 4. Current Native Architecture

| Module | Current responsibility | Fidelity status |
|---|---|---|
| `src/allstar_game.c` | Scene ownership and per-frame orchestration | Implemented structurally; not tied to ROM dispatcher states. |
| `src/scenes/` | Intro, menu, settings, roster, and game-mode scenes | Broad scaffolding; most game-mode rules are partial. |
| `src/gameplay/allstar_physics.c` | 60 Hz shot integration, rim-plane crossing, and court contacts | `$7BE8` gravity/integration, `$7EA9` normal-vector duration, `$1F4D` dead-ball stop, and two `$1CED` branches are represented; alternate launch, remaining contact, and bounce parity remain. |
| `src/gameplay/allstar_ai.c` | Six-state generic CPU controller | Prototype substitute for ROM player/CPU state machines. |
| `src/gameplay/allstar_one_on_one.c` | Match clocks, endings, overtime, possession, and staged shot state | High-level lifecycle is verified; staged shooting/traveling is partial pending exact ROM timing and physics traces. |
| `src/allstar_renderer.c` | Software pixels, tiles, court, players, and ball | Functional; several assets are compiled headers rather than asset-pack data. |
| `src/audio/allstar_audio.c` | Win32 PCM WAV mixer | Functional mixer; ROM sequencer and most sound events are missing. |
| `src/allstar_asset_pack.c` | Container serialization and basic tile decoding | Partial extraction pipeline. |

The actual frame tick is:

```c
void allstar_game_tick(AllStarGame *game, float dt) {
    game->active_scene->update(game->active_scene, game, &game->input, dt);
    allstar_audio_update(&game->audio, dt);
    game->active_scene->draw(game->active_scene, game, game->renderer);
    allstar_renderer_present(game->renderer);
}
```

Input is updated by the Win32 host before this call.

## 5. Game-Mode Status

| Flow or mode | Current native coverage | Important missing behavior |
|---|---|---|
| Title and menu | Partial | All five modes route correctly; the 1P/2P choice is not persisted. |
| Settings | Behavior verified | ROM defaults/cycles persist for the session and feed the relevant native modes; presentation parity remains partial. |
| Roster selection | Partial | Selection UI works; behavior and data are not yet verified against ROM tables. |
| One-on-One | Lifecycle verified; rules partial | Scoring, shot-clock possession, rebounds, winners-outs, staged shooting/traveling, `$7F37` release offsets, ROM court limits/return, and 32-frame rim crossings exist; exact control timing, remaining contact gates, alternate launch tables, steals, blocks, and contests remain. |
| Free Throws | Prototype | Correct basket parameters, configured attempt count, result state, and ROM timing model. |
| H-O-R-S-E | Prototype | Called-shot storage, matching attempts, CPU/human turns, letter rules, and win state. |
| Accuracy Shootout | Prototype/misidentified | The routed scene consumes time and position-source settings but remains a generic five-position contest. |
| Tournament | Gameplay flow verified | Winner propagation, four quarterfinals, two semifinals, final, champion lock, and title return are deterministic; presentation remains partial. |
| Two-player | Missing | Second input stream and two-human rules. Serial hardware transport is outside the native-port requirement. |

## 6. Audio Status

The ROM contains a multi-channel command/sequencing engine in bank 0, with confirmed APU-writing code in the approximate `$3014..$36DB` region. The command parser, timing, pitch/envelope behavior, song data, and event mapping remain to be reverse engineered.

The native port currently loads three BGM WAV files and two menu SFX WAV files. Tone generation and per-frame audio sequencing functions are no-ops, so gameplay sound events without loaded samples are silent.

## 7. Verification

### Current tests

```powershell
.\build\allstar_port.exe --test-all
```

| Test | What it proves | What it does not prove |
|---|---|---|
| `--rom-test` | Header parses and its header checksum is valid | Correct bank mapping or gameplay extraction. |
| `--test-roster` | Hardcoded roster count and two selected entries | ROM-derived roster fidelity. |
| `--test-physics` | Normal 32-frame launch, fixed-step chunk invariance, descending rim crossing, offset miss, `$1F4D` stop, and the `$1CED` back-court return | Alternate ROM trajectory tables, exact 8.8 traces, remaining rim/backboard contact, bounce, or rebound parity. |
| `--test-mode-routing` | All five ROM menu IDs reach the intended native scenes | Rules within those scenes. |
| `--test-settings` | ROM defaults/value cycles persist and affect the relevant mode state | Full mode or presentation parity. |
| `--test-one-on-one-lifecycle` | Endings, shot-clock turnover, overtime, result dismissal, exit, and tournament return | Detailed rules, physics, AI, or frame parity. |
| `--test-one-on-one-shooting` | Gather/release, `$7F37` offset cases, clean scoring, returned-miss recovery, `$077D` pickup limits, traveling, and possession resets | Exact ROM control timing, full animation sequencing, jump-height state, alternate trajectories, accuracy tables, contests, or emulator frame parity. |
| `--test-tournament` | All seven bracket matches propagate valid winners through champion and exit | Roster-selection or pixel-level bracket presentation parity. |
| `--test-headless-frames` | Seven scenes can tick and draw without crashing | Input flow, rules, results, visual parity, or completion. |

### Required parity layers

1. Script identical inputs in the emulator and native port.
2. Capture mode/state, important WRAM fields, positions, timers, scores, and RNG decisions.
3. Compare state at deterministic frame checkpoints.
4. Compare rendered frames with explicit tolerances.
5. Test complete mode flows, including results and return paths.

Manual side-by-side comparison is available with:

```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```

Manual visual inspection is useful for discovery but is not sufficient evidence for a verified routine mapping.
