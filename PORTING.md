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

The current headless export contains 16 functions:

- reset/interrupt vectors and thunks;
- the serial handler at `$0061`;
- boot/init at `$0100`/`$0150`;
- the VBlank handler at `$2729`.

The headless setup now creates and byte-verifies separate `$4000..$7FFF` overlays for physical banks 1, 2, and 3. No complete gameplay subsystem has yet been exported as a reliable Ghidra function set: the legacy decompiler pass still sweeps only the default `$0000..$7FFF` space and does not recover functions in the new overlays.

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

Project progress is tracked by 25 strict milestones in `docs/COVERAGE_MANIFEST.json`. Only `verified` milestones receive credit. The current checkpoint is **5/25 (20.00%)**, up from the audited **3/25 (12.00%)** baseline after verifying the bank 2 and bank 3 overlays. Verified gameplay parity is still **0/11 (0.00%)**.

## 4. Current Native Architecture

| Module | Current responsibility | Fidelity status |
|---|---|---|
| `src/allstar_game.c` | Scene ownership and per-frame orchestration | Implemented structurally; not tied to ROM dispatcher states. |
| `src/scenes/` | Intro, menu, settings, roster, and game-mode scenes | Broad scaffolding; most game-mode rules are partial. |
| `src/gameplay/allstar_physics.c` | Floating-point projectile and basket-radius check | Prototype substitute for ROM fixed-point physics. |
| `src/gameplay/allstar_ai.c` | Six-state generic CPU controller | Prototype substitute for ROM player/CPU state machines. |
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
| Title and menu | Partial | The 1P/2P choice is not persisted. Mode indices 1–3 route to the wrong scenes. |
| Settings | UI only | Play-to, difficulty, winners-outs, time, and attempt count are discarded on scene change. |
| Roster selection | Partial | Selection UI works; behavior and data are not yet verified against ROM tables. |
| One-on-One | Prototype | Match end, score target, shot-clock turnover, steals, blocks, collision, rule options, and results flow. |
| Free Throws | Prototype | Correct basket parameters, configured attempt count, result state, and ROM timing model. |
| H-O-R-S-E | Prototype | Called-shot storage, matching attempts, CPU/human turns, letter rules, and win state. |
| Accuracy Shootout | Missing/misidentified | Current `scene_three_point.c` is a generic five-rack contest and is not routed from the Accuracy option. |
| Tournament | Display prototype | Winner return, match advancement, round advancement, bracket mutation, and championship result. |
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
| `--test-physics` | A projectile can be initialized and advanced | ROM trajectory, rim interaction, or scoring parity. |
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
