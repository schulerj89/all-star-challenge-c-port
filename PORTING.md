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

   - Version 17 extracts the complete One-on-One court/player/ball data, Free Throw close-up shooter/background/OBJ data, all 24 animation-control lists, eleven focused ROM audio programs, and the original title/menu song.
   - The runtime no longer requires its former compiled derived-art header. Portraits, remaining-mode background graphics, roster records, other songs, and remaining audio still need migration behind the same boundary.

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
| Tiles | 2bpp planar, 16 bytes per 8×8 tile | One-on-One layout-specific extraction and composition are verified; other modes remain incomplete. |
| Sprites | Up to 40 OAM entries | Rendered as native sprite/player concepts rather than hardware OAM. |
| Audio | Two pulse, wave, and noise channels | Current port uses PCM mixing, decodes eleven focused ROM command programs, and renders the title/menu song's four channels; the whole music/sequencer engine is not implemented. |

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

The headless pipeline now creates byte-verified `$4000..$7FFF` overlays for physical banks 1, 2, and 3, then recovers a conservative seed set rather than sweeping unknown data as code. The validated inventory contains 139 functions that all create and decompile successfully:

- 83 fixed-bank functions in bank 0;
- 48 functions in bank 1's coherent `$6945..$7FFF` gameplay region;
- 8 functions in bank 2's reviewed `$4000..$42A1` code region.

The generated C listing contains 152 functions when reset/interrupt vectors and thunks are included. Bank 3 has no reviewed function seeds yet: current references select it as an asset-copy source, but a complete code/data review is still required. Bank 1 `$76A7` is recorded as a deferred call-target candidate because unresolved flows make Ghidra build an invalid oversized body and time out.

`tools/ghidra/function_seeds.json` is the checked-in symbol/native-status source. The latest inventory records 43 functions as unmapped, 38 as candidate native analogues, and 58 as verified for narrowly tested behavior.

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

Project progress is tracked by 25 strict milestones in `docs/COVERAGE_MANIFEST.json`. Only `verified` milestones receive credit. The current checkpoint is **13/25 (52.00%)**, up from the audited **3/25 (12.00%)** baseline. Analysis is **6/7 (85.71%)** and verified gameplay parity is **7/11 (63.64%)**.

One-on-One gameplay has a completed fixed denominator in `docs/parity/ONE_ON_ONE_COVERAGE.json`: **50/50 (100.00%)**. The second fixed denominator in `docs/parity/ONE_ON_ONE_REMAINING_COVERAGE.json` is also complete at **50/50 (100.00%)**, comprising RNG **10/10**, animation/assets **20/20**, and collision/reaction **20/20**. Frame-perfect synchronization remains deliberately excluded. Check both manifests with `python tools/check_one_on_one_coverage.py` and `python tools/check_one_on_one_remaining_coverage.py`.

## 4. Current Native Architecture

| Module | Current responsibility | Fidelity status |
|---|---|---|
| `src/allstar_game.c` | Scene ownership and per-frame orchestration | Implemented structurally; not tied to ROM dispatcher states. |
| `src/scenes/` | Intro, menu, settings, roster, and game-mode scenes | Broad scaffolding; most game-mode rules are partial. |
| `src/gameplay/allstar_physics.c` | 60 Hz shot integration, rim-plane crossing, and court contacts | `$7BE8` uses exact 8.8 gravity/friction/integration operations; `$7EA9` normal-vector duration, `$1F4D` dead-ball stop, and two `$1CED` branches are represented; alternate launch, remaining contact, and bounce parity remain. |
| `src/gameplay/allstar_ai.c` | `$7170-$761A` CPU mode dispatch, targets, decisions, defense, contact response, synthetic input, and shared ROM RNG consumption | The complete recovered controller is verified; modes 0/4 use the full play path, modes 1/3 return, and mode 2 uses the `$74A8/$756C` target/gather/release path. |
| `src/gameplay/allstar_rng.c` | Fixed `$0714/$072F` shared-frame RNG and BCD entropy | Exact low-byte recurrence/cadence is Ghidra- and Mesen-verified. |
| `src/gameplay/allstar_one_on_one.c` | Match clocks, possession, shots, shared `$702D/$714D` player control, `$782E/$6A8C/$6B72` movement, and `$2C50/$2CCA/$0AC5` contact rules | The scoped One-on-One lifecycle, complete player controller, animation, direct movement, contact latch, and charging/blocking paths are verified. |
| `src/gameplay/allstar_free_throw.c` | `$0C8E/$100F` Free Throw attempts, aim, launch, fixed-point flight, rim/make, net, and scoring | Expanded gameplay/presentation is 32/32 and Mesen/native traced, including single-player selection, `$0C8E` music stop, `$22A9/$1A25` reticle, `$1A7E` rating make tables, `$1C1D` priority, and `$C12B` gravity hold. |
| `src/gameplay/allstar_horse.c` | `$0CDF/$0D57/$0E26/$0E36/$6CAB/$7BC0` Horse rules and CPU spots | 30/30 scoped requirements are native and traced; 27/45 full shared Ghidra routines have strict verified credit. |
| `src/allstar_renderer.c` | Software pixels and ROM-derived court/player/ball composition | One-on-One `$2945/$2A2B` player and `$6945/$69F5` ball/shadow paths consume the user-built pack; Free Throw's separate renderer is in `scene_free_throw.c`. |
| `src/sdl_game_main.c` | SDL 3 video, keyboard/gamepad input, frame pacing, and app lifecycle | Shared host builds on macOS, Windows, and Linux; iOS packaging and touch controls remain. |
| `src/audio/allstar_audio.c` | Shared PCM mixer with SDL and Win32 output backends | Eleven focused ROM commands plus the title/menu song are decoded, including Accuracy `$02`, Horse `$07`, and Free Throw `$08/$0A`; the complete sequencer/music engine remains partial. |
| `src/allstar_asset_pack.c` | Versioned container, graphics/animation extraction, and focused audio decoding | Version 17 validates One-on-One art, exact Free Throw `$2243/$1CBD` art/maps, Horse X reuse, eleven ROM audio programs, and the title/menu song; portraits and other songs remain partial. |

The actual frame tick is:

```c
void allstar_game_tick(AllStarGame *game, float dt) {
    game->active_scene->update(game->active_scene, game, &game->input, dt);
    allstar_audio_update(&game->audio, dt);
    game->active_scene->draw(game->active_scene, game, game->renderer);
    allstar_renderer_present(game->renderer);
}
```

Input is updated by the SDL host (or the optional Win32 reference host) before
this call.

## 5. Game-Mode Status

| Flow or mode | Current native coverage | Important missing behavior |
|---|---|---|
| Title and menu | Partial | All five modes route correctly; the 1P/2P choice is not persisted. |
| Settings | Behavior verified | ROM defaults/cycles persist for the session and feed the relevant native modes; presentation parity remains partial. |
| Roster selection | Partial | Selection UI works; behavior and data are not yet verified against ROM tables. |
| One-on-One | Gameplay 50/50; remaining focus 50/50 | Rules, shooting, steals, contests, recovery, RNG, complete `$702D` player input, complete `$7170` CPU decisions, animation records, direct movement/contact, charging/blocking, and ROM-derived court/player/ball presentation are verified. Frame-perfect synchronization remains excluded. |
| Free Throws | Gameplay/presentation 32/32 | Bank-2 single-player selection bypasses VS; `$0C8E` stops menu music; `$22A9/$1A25` draws the moving reticle; lifecycle, aim/launch, rating-based clean makes, rim/net score, results, exact close-up assets, and `$1C1D` priority are playable. |
| H-O-R-S-E | 30/30 scoped verified | Complete caller/matcher, CPU/human turns, exact spots/X/letters, shared shooting, command `$07`, winner, and exit; strict shared-routine coverage is 27/45. |
| Accuracy Shootout | Prototype/misidentified | The routed scene consumes time and position-source settings but remains a generic five-position contest. |
| Tournament | Gameplay flow verified; ROM ledger 64/64 | Winner propagation, four quarterfinals, two semifinals, final, champion lock, and title return are deterministic. All 64 mode-exclusive ROM routines (2,961 bytes) are ported and tracked in `docs/parity/TOURNAMENT_ROM_COVERAGE.json`; screen-art parity remains partial. |
| Two-player | Missing | Second input stream and two-human rules. Serial hardware transport is outside the native-port requirement. |

## 6. Audio Status

The ROM contains a multi-channel command/sequencing engine in bank 0, with confirmed APU-writing code in the approximate `$3014..$36DB` region. Eleven focused commands are decoded through the command/program/instrument tables, including Free Throw net `$08` and contact `$0A`. The title/menu song is decoded from its control, note, instrument, wave-table, and loop data into version 19 of the user-built asset pack, together with its per-frame NR51 routing -- `$35B6` places the two square voices on opposite sides, so the song is genuinely stereo. Each cue's NR12 envelope is applied when rendered rather than held flat. Both were measured against Mesen captures of the cartridge's own APU writes; see `docs/parity/TITLE_MUSIC.md` and `docs/parity/SFX_ENVELOPE.md`. Other songs, the general-purpose sequencer, and the remaining event map still require reverse engineering.

The native mixer renders the packed title/menu song through two pulse channels, the wave channel, and noise at Game Boy frame timing, then loops from the state-derived loop point. Optional WAV files can still supply other tracks; decoded ROM cues replace native fallback effects when the pack is loaded.

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
| `--test-one-on-one-shooting` | Gather/release, `$7F37` offsets, exact `$0714/$072F` RNG, `$6A8C/$6C60` animation state, `$6B72/$6E3C` movement/contact, `$2C50/$2CCA/$0AC5` violations, `$75CD` CPU response, player/ball composition, steals, contacts, recovery gates, traveling, and possession resets | Other modes or full frame parity. |
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
