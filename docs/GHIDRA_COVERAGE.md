# NBA All-Star Challenge — Ghidra-to-C Coverage Audit

Last audited: **2026-08-14**

## Executive Result

For the current reviewed function inventory, verified Ghidra-to-C routine coverage is **0/83 (0.00%)**. All 83 entry points have stable bank-aware symbols and decompile successfully; 80 are explicitly unmapped and three have candidate native analogues without parity evidence. This is a conservative reviewed subset, not a claim that the entire ROM contains only 83 functions.

The previous `108/108 (100%)` figure was produced from a hand-written table and a token-presence checker; it did not establish that the ROM routines were identified correctly or reproduced in C.

The port currently has broad native scene scaffolding. Mode routing and the high-level One-on-One lifecycle are now verified against reviewed fixed-bank control flow; most detailed game rules remain partial, simplified, or unverified.

The strict project milestone tracker is currently **8/25 (32.00%)**, increased from the audited **3/25 (12.00%)** baseline. Analysis is **6/7 (85.71%)** and gameplay is **2/11 (18.18%)** verified.

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
| Reviewed Ghidra functions | 83 functions | 29 in bank 0, 46 in bank 1, and 8 in bank 2; every seed creates and decompiles successfully. |
| Generated Ghidra C export | 96 functions | Reviewed functions plus reset/interrupt vectors and thunks. |
| Reviewed routine-to-C parity | 0 of 83 | Three candidate analogues exist, but none has trace/test evidence. |
| `mgbdis` `Call_*` labels | 246 | Candidate entry points; some may be false code. |
| `mgbdis` `Jump_*` labels | 116 | Branch targets, not necessarily standalone functions. |
| Unique direct `call` targets | 251 | Analysis queue, not a verified function denominator. |
| Rows in the former detailed matrix | 43 | The former summary claimed 108 without listing 108 mappings. |
| Former matrix identifiers found verbatim in assembly | 33 of 43 | Identifier existence still does not prove the stated meaning. |
| Current focused behavior checks | 2 | Mode routing and One-on-One lifecycle tests encode reviewed ROM control-flow expectations; emulator state/frame comparison remains missing. |

### Reviewed Ghidra recovery

`tools/ghidra/setup_banked_rom.py` creates and verifies the bank overlays. `recover_banked_functions.py` then consumes `function_seeds.json`, creates stable names such as `rom_b01_69f5`, decompiles each reviewed seed, and writes `build/ghidra_function_inventory.json`. `tools/check_ghidra_functions.py` rejects missing functions, wrong bank spaces, empty bodies, decompiler failures, and invalid native mapping records.

The current conservative boundaries are:

- bank 0: 29 boot and cross-bank anchor routines;
- bank 1: coherent code beginning at `$6945`, with generated labels before that region excluded as likely data false positives;
- bank 2: code at `$4000..$42A1`, followed by a visible data boundary at `$42A2`;
- bank 3: no reviewed functions yet; observed uses are asset-copy sources.

Bank 1 `$76A7` is a confirmed call target but remains deferred because Ghidra follows unresolved flows into an oversized invalid function and times out. It receives no recovered-function or coverage credit.

### Percentage tracker

`docs/COVERAGE_MANIFEST.json` defines 25 fixed, all-or-nothing milestones. Identified, scaffolded, and partial items receive no percentage credit. Each verified milestone contributes 4 percentage points.

| Checkpoint | Verified | Percentage | Delta |
|---|---:|---:|---:|
| Audited baseline | 3/25 | 12.00% | — |
| Four-bank mapping | 5/25 | 20.00% | +8.00 points |
| Reviewed symbol inventory | 6/25 | 24.00% | +4.00 points |
| Mode routing + One-on-One lifecycle | 8/25 | 32.00% | +8.00 points |

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
| One-on-One | Yes | Lifecycle only | Endings, overtime, result dismissal, exit, and tournament return are covered; rules remain partial. |
| Free Throws | Yes | No | Broken/incomplete prototype |
| H-O-R-S-E | Yes | No | Incomplete prototype |
| Accuracy Shootout | No faithful implementation | No | Misidentified as a generic five-rack contest |
| Tournament | Yes | One-on-One return only | Winner return and bracket advancement work; complete tournament parity remains unverified. |
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
| P0 | Four-bank Ghidra function recovery | 83 stable functions are recovered; finish bank 0/1 boundaries, resolve `$76A7`, review bank 3, and export call graphs/memory references. |
| P0 | Coverage tooling | Milestone and routine inventories exist; add deterministic trace/test evidence before promoting any candidate mapping to verified. |
| P0 | Settings | Persist play-to, difficulty, winners-outs, time, and attempt count into the game state and consume them in each mode. |
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

The manifest now exists, so report its verified subset explicitly: **0/83 routine mappings verified**. Do not use 83 as a whole-ROM function denominator until the remaining code/data review is complete.
