# NBA All-Star Challenge — Ghidra-to-C Coverage Audit

Last audited: **2026-08-15**

## Executive Result

For the current reviewed function inventory, verified Ghidra-to-C routine coverage is **8/100 (8.00%)**. All 100 entry points have stable bank-aware symbols and decompile successfully; 66 are explicitly unmapped, 26 have candidate native analogues, and eight narrowly scoped mappings are verified by deterministic tests and traces: `$0714/$072F` shared-frame RNG, `$077D` loose-ball collision, `$0A78` protected actions, `$1F4D` planar dead-ball stop, `$28E1` unsigned score comparison, `$2B14` steals, and `$2B6C` defensive-jump recovery. `$6A8C` remains a candidate even though its complete `$6C60` record engine is now ported: movement side effects and all action-selection callers are not yet equivalent. `$702D` likewise remains a candidate backed by emulator-matched subsets. This is a conservative reviewed subset, not a claim that the entire ROM contains only 100 functions.

The previous `108/108 (100%)` figure was produced from a hand-written table and a token-presence checker; it did not establish that the ROM routines were identified correctly or reproduced in C.

The port currently has broad native scene scaffolding. Mode routing, settings persistence/consumption, the high-level One-on-One lifecycle, and the seven-match tournament progression are verified against reviewed fixed-bank control flow; most detailed game rules remain partial, simplified, or unverified.

The strict project milestone tracker is currently **10/25 (40.00%)**, increased from the audited **3/25 (12.00%)** baseline. Analysis is **6/7 (85.71%)** and gameplay is **4/11 (36.36%)** verified.

The separate One-on-One behavior manifest is **50/50 (100.00%)**. Its fixed requirements are now complete, including steals, defensive jumps, post-contact recovery, and the ROM's explicit initial-flight lock/no-separate-goaltending behavior. This focused result does not promote incomplete whole-project milestones or candidate whole-routine mappings.

The separate remaining-focus manifest is **26/50 (52.00%)**: exact RNG is **10/10**, animation/assets are **8/20**, and collision/reaction recovery is **8/20**. It intentionally tracks work excluded from the completed 50-item gameplay subset.

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
| Reviewed Ghidra functions | 100 functions | 45 in bank 0, 47 in bank 1, and 8 in bank 2; every seed creates and decompiles successfully. |
| Generated Ghidra C export | 113 functions | Reviewed functions plus reset/interrupt vectors and thunks. |
| Reviewed routine-to-C parity | 8 of 100 | `$0714`, `$072F`, `$077D`, `$0A78`, `$1F4D`, `$28E1`, `$2B14`, and `$2B6C` are verified for their narrow tested semantics; 26 candidate analogues remain below whole-routine evidence threshold. |
| `mgbdis` `Call_*` labels | 246 | Candidate entry points; some may be false code. |
| `mgbdis` `Jump_*` labels | 116 | Branch targets, not necessarily standalone functions. |
| Unique direct `call` targets | 251 | Analysis queue, not a verified function denominator. |
| Rows in the former detailed matrix | 43 | The former summary claimed 108 without listing 108 mappings. |
| Former matrix identifiers found verbatim in assembly | 33 of 43 | Identifier existence still does not prove the stated meaning. |
| Current focused behavior checks | 8 | Mode routing, settings, One-on-One lifecycle, staged shooting/traveling, tournament progression, and headless input, defense, and 32-frame RNG traces are checked; broader frame/state comparison remains missing. |

### Reviewed Ghidra recovery

`tools/ghidra/setup_banked_rom.py` creates and verifies the bank overlays. `recover_banked_functions.py` then consumes `function_seeds.json`, creates stable names such as `rom_b01_69f5`, decompiles each reviewed seed, and writes `build/ghidra_function_inventory.json`. `tools/check_ghidra_functions.py` rejects missing functions, wrong bank spaces, empty bodies, decompiler failures, and invalid native mapping records.

The current conservative boundaries are:

- bank 0: 45 boot, RNG, cross-bank anchor, One-on-One lifecycle, contact, steal, and recovery routines;
- bank 1: 47 functions in coherent code beginning at `$6945`, including the `$6A8C` animation dispatcher; generated labels before that region remain excluded as likely data false positives;
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
| Settings persistence and consumption | 9/25 | 36.00% | +4.00 points |
| Tournament bracket and championship | 10/25 | 40.00% | +4.00 points |

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
| Settings UI | Yes | Scoped behavior | ROM defaults/cycles persist for the session and feed mode state; presentation parity remains partial. |
| Roster selection | Yes | No | Partial; data remains hardcoded |
| One-on-One | Yes | 50/50 gameplay; 26/50 remaining focus | Lifecycle, possession, shooting, contacts, CPU decisions, steals, contests, recovery, RNG, and `$6C60` animation records are covered. Remaining focus is directional selection, graphics extraction, and player collision-reaction behavior. |
| Free Throws | Yes | No | Broken/incomplete prototype |
| H-O-R-S-E | Yes | No | Incomplete prototype |
| Accuracy Shootout | No faithful implementation | No | Misidentified as a generic five-rack contest |
| Tournament | Yes | Scoped gameplay flow | Four quarterfinals, two semifinals, final, champion lock, and exit are covered; presentation remains partial. |
| Two-player gameplay | No | No | Missing |
| Ball physics | Yes | Scoped One-on-One flight/contact | Exact 60 Hz 8.8 flight, 32/64-frame launch vectors and tables, `$1CED` score/rim/backboard cells, `$1E77` bounce loss, outer/back-court response, and `$1F4D` stop are deterministic; other modes and upstream accuracy/point selection remain incomplete. |
| Player collision and possession rules | Partial | Focused One-on-One subset | Shot-clock, rebound possession, winners-outs, traveling, exact `$077D` pickup limits, `$2B14` steals, and `$2B6C` post-contact jump recovery exist; broader collision penalties/reactions remain missing. |
| CPU AI | Yes | Scoped One-on-One behavior | `$72EA` targets, `$74BB` direction dead zones, `$756C` profile/skill shot choice, `$71B3/$762C` steal thresholds, `$71EE` contest gate, and exact shared `$0714/$072F` RNG are integrated; collision and the rest of `$7170` remain incomplete. |
| Player animation selection | Yes | Complete record lists; partial callers/art | `$6A8C/$6C60` record timing, idle/steal live paths, shot phase overrides, and `$6C4D` jump heights are verified. Directional caller selection and extracted frame art remain missing. |
| Court/menu/player rendering | Yes | No | Functional presentation; much data is compiled into headers |
| ROM asset extraction | Minimal | No | Fixed tile range plus hardcoded roster |
| PCM output/mixing | Yes | Not applicable to original implementation | Native platform layer |
| ROM music/SFX sequencing | No | No | Missing |
| Emulator/state/frame parity suite | No | No | Missing |

No whole subsystem should currently be labeled 100% ROM-equivalent; the
50/50 figure is explicitly a fixed, focused One-on-One behavior manifest.

## Missing-Work Matrix

| Priority | Area | Required work for verified coverage |
|---|---|---|
| P0 | Four-bank Ghidra function recovery | 100 stable functions are recovered; finish bank 0/1 boundaries, resolve `$76A7`, review bank 3, and export call graphs/memory references. |
| P0 | Coverage tooling | Milestone and routine inventories exist; add deterministic trace/test evidence before promoting any candidate mapping to verified. |
| Complete | Settings | Session persistence, ROM value cycles, and downstream consumption are verified; full Accuracy and Free Throw mode parity remain tracked separately. |
| Complete | One-on-One scoped rules | 50/50 fixed requirements are verified, including steals and the recovered contest/jump-recovery/no-goaltending behavior; broader collision and presentation work stays in separate milestones. |
| P0 | Free Throws | Correct the shot/basket coordinate model, enforce the configured attempt count, and implement results/exit flow. |
| Complete | Tournament | Seven-match winner propagation, bracket rounds, champion lock, and title return are verified; presentation remains outside this gameplay milestone. |
| P1 | H-O-R-S-E | Store the called shot, require a matching attempt, apply letters to the matching player, support CPU/human turns, and end the game. |
| P1 | Accuracy Shootout | Identify and port the actual ROM rules, target sequence, timer, scoring, and end state. |
| P1 | Two-player game logic | Preserve the 1P/2P selection and add a second native input stream and two-human state flow. Serial hardware transport remains excluded. |
| P1 | Physics | One-on-One launch/contact/bounce is covered; recover the upstream `$FFD7` 2/3-point selector and accuracy-to-launch-index logic, then extend equivalent evidence to other modes. |
| P1 | AI | One-on-One targets, profile/skill shot selection, steal thresholds, and contest gating are covered; recover collision penalties and remaining `$7170` states. |
| P1 | Animation | Recover ROM animation/state tables and map movement, dribble, gather, shot, block, rebound, hit, and idle sequences. |
| P1 | Audio sequencing | Recover the bank-0 audio command interpreter, song/SFX tables, channel timing, pitch, envelope, and event mappings. |
| P1 | Asset pipeline | Extract actual tile regions, tilemaps, portraits, logos, animations, roster records, and audio data from the user ROM. |
| P1 | Runtime asset boundary | Replace compiled derived-art arrays with asset-pack data loaded at runtime. |
| P1 | Parity tests | Scripted input/defense/RNG traces and deterministic native RNG now exist; add broader WRAM/native-state checkpoints and frame comparisons. |

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

The manifest now exists, so report its verified subset explicitly: **8/100 routine mappings verified**. Do not use 100 as a whole-ROM function denominator until the remaining code/data review is complete. The scoped remaining-focus percentage is checked separately with `python tools/check_one_on_one_remaining_coverage.py`.
