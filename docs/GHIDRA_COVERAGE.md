# NBA All-Star Challenge — Ghidra-to-C Coverage Audit

Last audited: **2026-08-14**

## Executive Result

Authentic routine-level coverage is **not yet measurable**. The previous `108/108 (100%)` figure was produced from a hand-written table and a token-presence checker; it did not establish that the ROM routines were identified correctly or reproduced in C.

The port currently has broad native scene scaffolding, but most game-mode behavior is partial, simplified, or unverified against the ROM.

The strict project milestone tracker is currently **5/25 (20.00%)**, increased from the audited **3/25 (12.00%)** baseline by completing byte-verified overlays for physical banks 2 and 3. Gameplay milestones remain **0/11 (0.00%) verified**.

## Scope

This report tracks game behavior that belongs in a native port. The following Game Boy implementation details are intentionally excluded from the gameplay denominator:

- reset-vector and interrupt prologues;
- MBC1 register writes;
- VRAM/OAM transfer mechanics and OAM DMA;
- raw joypad-register polling;
- serial-link transport mechanics;
- direct PPU and APU register writes.

Game rules, two-player state synchronization after transport, audio command sequencing, physics, AI, animation selection, menus, roster interpretation, scoring, timers, and mode progression remain in scope.

## Audited Baseline

| Measurement | Verified result | Meaning |
|---|---:|---|
| ROM size | 65,536 bytes | Four 16 KiB banks, not a 32 KiB flat ROM. |
| Cartridge type | `0x01` — MBC1 | Banks 1–3 share the CPU window `$4000..$7FFF`. |
| MBC1 bank mapping | 4 of 4 physical banks | Bank 0 is fixed; banks 1–3 are byte-verified overlays at `$4000..$7FFF`. |
| Current Ghidra export | 16 functions | Mostly reset/interrupt vectors, thunks, boot/init, serial, and VBlank; overlay function recovery remains. |
| `mgbdis` `Call_*` labels | 246 | Candidate entry points; some may be false code. |
| `mgbdis` `Jump_*` labels | 116 | Branch targets, not necessarily standalone functions. |
| Unique direct `call` targets | 251 | Analysis queue, not a verified function denominator. |
| Rows in the former detailed matrix | 43 | The former summary claimed 108 without listing 108 mappings. |
| Former matrix identifiers found verbatim in assembly | 33 of 43 | Identifier existence still does not prove the stated meaning. |
| Current automated parity tests | 0 | Existing tests are native smoke tests, not ROM comparisons. |

### Ghidra limitation

`tools/ghidra/setup_banked_rom.py` now creates the correct bank overlays and verifies mapped bytes against the physical ROM file. `tools/ghidra/decompile_all.py` still sweeps only the default `$0000..$7FFF` space, so it has not yet disassembled or recovered functions in the three switchable-bank overlays.

The exported functions are:

- `vec_0000`, `vec_0008`, `vec_0010`, `vec_0018`, `vec_0020`, `vec_0028`, `vec_0038`;
- VBlank, LCD, timer, serial, and joypad vector functions/thunks;
- `FUN_0061` for serial handling;
- boot/init at `$0100` and `$0150`;
- `FUN_2729` for the VBlank handler.

This is not enough to calculate routine-level gameplay coverage.

### Percentage tracker

`docs/COVERAGE_MANIFEST.json` defines 25 fixed, all-or-nothing milestones. Identified, scaffolded, and partial items receive no percentage credit. Each verified milestone contributes 4 percentage points.

| Checkpoint | Verified | Percentage | Delta |
|---|---:|---:|---:|
| Audited baseline | 3/25 | 12.00% | — |
| Four-bank mapping | 5/25 | 20.00% | +8.00 points |

Run `python tools/check_coverage.py` to validate and report the current checkpoint. For future changes, use `python tools/check_coverage.py --baseline-ref HEAD --require-delta 2` to enforce the two-point commit gate against the currently committed manifest. The initial 12%→20% checkpoint uses the recorded history because the manifest did not exist at the prior Git revision.

## Why the Former 100% Result Was Invalid

The former checker in `tools/check_coverage.py` marked a row implemented when either the proposed C identifier or ROM label appeared anywhere in `src/` or `include/`. This allowed comments, declarations, no-op functions, generic functions, and unrelated constants to count as complete routine ports. It has been replaced by the strict milestone-manifest validator.

Examples of disproved or unsupported mappings from the former matrix:

| Former claim | Assembly evidence | Audit result |
|---|---|---|
| `$0002` is the sound-driver tick | The bytes at `$0002` are repeated `rst RST_38` padding | Incorrect. |
| `$000C` is a sound-frequency routine | `$000C` is `jp hl`, an indirect-call trampoline | Incorrect. |
| `$0078` starts BGM | It restores registers and executes `reti` | Incorrect. |
| `$007B` stops BGM | It reads `rSB` and processes serial state | Incorrect. |
| `$347B` verifies the ROM header | It manipulates the sound engine's WRAM state in the `$30xx..$36xx` audio region | Incorrect. |
| `$447C` decompresses portraits | The labeled bytes decode as implausible instruction/data noise and have not been established as a function | Unsupported. |
| Generic `allstar_ai_update` covers several ROM AI routines | No state/output comparison exists | Conceptual analogue only. |
| Generic projectile functions cover ROM shot physics | No fixed-point/state/trajectory comparison exists | Conceptual analogue only. |

Ten former identifiers do not appear verbatim in the generated assembly:

`Call_001_7170`, `Jump_001_71ca`, `Call_001_7ba6`, `Call_001_6a5c`, `Call_000_2100`, `Call_000_2300`, `Call_000_2400`, `Bank2_0x4000`, `Bank3_0x708E`, and `Bank3_0x6EF1`.

Some corresponding addresses may still contain real code or data. The point is that the former names were not grounded in the checked disassembly.

## Native Feature Coverage

| Subsystem | Native code exists | ROM-equivalent behavior verified | Current assessment |
|---|:---:|:---:|---|
| Runtime/scene orchestration | Yes | No | Structural implementation |
| Input edge detection | Yes | No | Native utility; raw hardware polling excluded |
| Intro/title/menu presentation | Yes | No | Partial |
| Settings UI | Yes | No | UI only; values are discarded |
| Roster selection | Yes | No | Partial; data remains hardcoded |
| One-on-One | Yes | No | Gameplay prototype |
| Free Throws | Yes | No | Broken/incomplete prototype |
| H-O-R-S-E | Yes | No | Incomplete prototype |
| Accuracy Shootout | No faithful implementation | No | Misidentified as a generic five-rack contest |
| Tournament | Display only | No | Progression missing |
| Two-player gameplay | No | No | Missing |
| Ball physics | Yes | No | Generic floating-point substitute |
| Player collision and possession rules | Minimal | No | Mostly missing |
| CPU AI | Yes | No | Generic six-state substitute |
| Player animation selection | Yes | No | Limited generic states |
| Court/menu/player rendering | Yes | No | Functional presentation; much data is compiled into headers |
| ROM asset extraction | Minimal | No | Fixed tile range plus hardcoded roster |
| PCM output/mixing | Yes | Not applicable to original implementation | Native platform layer |
| ROM music/SFX sequencing | No | No | Missing |
| Emulator/state/frame parity suite | No | No | Missing |

No subsystem should currently be labeled 100% ROM-equivalent.

## Missing-Work Matrix

| Priority | Area | Required work for verified coverage |
|---|---|---|
| P0 | Four-bank Ghidra function recovery | Bank overlays are complete; recover functions in each overlay, separate code from data, and export stable symbols/call graphs. |
| P0 | Coverage tooling | Milestone tracking is complete; add the routine-level mapping manifest after stable banked functions exist. |
| P0 | Mode routing | Align the menu's Free Throws, H-O-R-S-E, and Accuracy selections with their intended scenes. |
| P0 | Settings | Persist play-to, difficulty, winners-outs, time, and attempt count into the game state and consume them in each mode. |
| P0 | One-on-One lifecycle | Add score/time endings, shot-clock turnover, results, replay/exit flow, and tournament return. |
| P0 | One-on-One rules | Port possession changes, steals, blocks, collisions, rebounding, shot contest, winners-outs, and difficulty behavior. |
| P0 | Free Throws | Correct the shot/basket coordinate model, enforce the configured attempt count, and implement results/exit flow. |
| P0 | Tournament | Record winners, advance matches and rounds, mutate the bracket, and complete the championship flow. |
| P1 | H-O-R-S-E | Store the called shot, require a matching attempt, apply letters to the matching player, support CPU/human turns, and end the game. |
| P1 | Accuracy Shootout | Identify and port the actual ROM rules, target sequence, timer, scoring, and end state. |
| P1 | Two-player game logic | Preserve the 1P/2P selection and add a second native input stream and two-human state flow. Serial hardware transport remains excluded. |
| P1 | Physics | Replace or validate the generic parabola against ROM fixed-point movement, release timing, rim/backboard contact, bounce, and rebound behavior. |
| P1 | AI | Recover CPU state transitions, difficulty tables, player-rating effects, defense, steals, blocks, shot selection, and rebound logic. |
| P1 | Animation | Recover ROM animation/state tables and map movement, dribble, gather, shot, block, rebound, hit, and idle sequences. |
| P1 | Audio sequencing | Recover the bank-0 audio command interpreter, song/SFX tables, channel timing, pitch, envelope, and event mappings. |
| P1 | Asset pipeline | Extract actual tile regions, tilemaps, portraits, logos, animations, roster records, and audio data from the user ROM. |
| P1 | Runtime asset boundary | Replace compiled derived-art arrays with asset-pack data loaded at runtime. |
| P1 | Parity tests | Add scripted emulator/native inputs, WRAM/native-state checkpoints, deterministic RNG handling, and frame comparisons. |

## Coverage Status Rules

Future mappings should use these labels:

| Status | Required evidence |
|---|---|
| Unmapped | No confirmed native counterpart. |
| Identified | ROM routine boundaries and purpose are supported by code/data and cross-reference evidence. |
| Scaffolded | A native API or scene exists, but behavior is not equivalent. |
| Partial | Some important state transitions match; known behavior remains missing. |
| Verified | Deterministic test or trace demonstrates equivalent inputs, state transitions, and outputs for the stated scope. |

Each verified mapping should record:

1. physical bank and CPU address;
2. confirmed routine boundaries;
3. inputs and relevant WRAM/HRAM state;
4. state changes and outputs;
5. native C symbol and source file;
6. emulator trace or comparison artifact;
7. automated test identifier;
8. known deviations.

Until that manifest exists, report structural and feature status rather than a numeric routine-coverage percentage.
