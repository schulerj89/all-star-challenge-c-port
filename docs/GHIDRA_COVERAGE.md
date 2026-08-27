# NBA All-Star Challenge — Ghidra-to-C Coverage Audit

Last audited: **2026-08-16**. Amended **2026-08-26**, see below.

## Amendment, 2026-08-26: the denominator was wrong

An independent pass rebuilt the ROM byte-exactly from `disassembly/bank_*.asm`
and walked it by recursive descent from the reset and interrupt vectors,
tracking the MBC1 bank through the `rst $20` trampoline and decoding the
`rst $08` / `rst $10` pointer tables. That produces a routine inventory that
does not depend on what anyone had already noticed, and it says:

- The cartridge holds about **17.3 KB of executable code in ~310 routines**.
  The other three quarters of the 64 KB is graphics and audio data that
  `mgbdis` prints as instructions. **Bank 3 contains no executable code at
  all**, and bank 2 has only its `$4000` selector family.
- `tools/ghidra/function_seeds.json` lists 145 addresses. Against that trace,
  127 are genuine routine entry points, 11 land mid-routine, and 7 are not
  statically reachable — those seven being the Accuracy HUD writers found with
  Mesen. **165 routines that do execute are absent from the file.**

So every percentage in this document with 145 in the denominator understates
the work by roughly a factor of two. The figures below are still accurate about
the reviewed subset; they are simply not a measure of the cartridge.

The largest single gap that pass found was **menu mode `$04`, the tournament**:
64 routines and 2,961 bytes reachable from no other mode, with almost no native
implementation. That is being ported in ~10% increments and tracked in
`parity/TOURNAMENT_ROM_COVERAGE.json`, which counts a routine only when a named
source file references its address. `parity/TOURNAMENT.md` records what the
disassembly established, including several behaviours the earlier scaffolding
had wrong.

### The button packing is now settled from hardware

`$2639`, the joypad poll, is ported (`parity/JOYPAD.md`). It selects the
direction row, `swap`s it into the high nibble, then ORs the button row in low,
so the assembled byte is bit 0 A, 1 B, 2 Select, 3 Start, 4 Right, 5 Left,
6 Up, 7 Down. Every raw mask elsewhere in the port was inferred from usage
before this; `--test-pad` now asserts each of them against the layout the
hardware read produces, and they all agree. Note that this is **not**
`AllStarButtonMask`, which is very nearly its reverse.

## Executive Result

For the current reviewed function inventory, verified Ghidra-to-C routine coverage is **67/145 (46.21%)**. All 145 entry points have stable bank-aware symbols and decompile successfully; 40 are explicitly unmapped, 38 have candidate native analogues, and 67 mappings are verified by deterministic tests and traces. Accuracy now includes the independently recovered `$7749/$7765/$7790/$77A1` HUD writers in addition to fixed `$0B20/$0E51` and its `$6C9B/$6CA2/$6D57` controller helpers. This is a reviewed subset, not a claim that the entire ROM contains only 145 functions.

The previous `108/108 (100%)` figure was produced from a hand-written table and a token-presence checker; it did not establish that the ROM routines were identified correctly or reproduced in C.

The port currently has broad native scene scaffolding. Mode routing, settings persistence/consumption, the high-level One-on-One lifecycle, and the seven-match tournament progression are verified against reviewed fixed-bank control flow; most detailed game rules remain partial, simplified, or unverified.

The strict project milestone tracker is currently **14/25 (56.00%)**, increased from the audited **3/25 (12.00%)** baseline. Analysis is **6/7 (85.71%)** and gameplay is **8/11 (72.73%)** verified.

The one-player Accuracy Shootout manifest is **25/25 (100.00%)**. Its
Ghidra route is `$4000->$4034->$0E51->$6C9B->$6CA2/$6D57->$7AFD->$702D
->$7C58->$0EE7/$0F1E->$0FDE`, with `$76A7->$7739->$7749/$7765/$7790/$77A1
->$780A` maintaining the live court-panel HUD. The strict eleven-routine
Accuracy-specific inventory is 8 verified and 3 candidate (**72.73% whole-routine coverage,
100.00% mapped**); candidate credit remains conservative where the same ROM
routine has other callers or unrequested two-human state. Mesen proves the
single-selector route, exact first target, four exact HUD tile destinations,
attempts/makes, +80-frame make commit, and command `$02` APU output.

The H-O-R-S-E gameplay manifest is **30/30 (100.00%)**. The original trace
follows `$22EF->$255D->$4000->$0CDF->$0D57->$7AEA/$7AFD->$7C58->$0E26->$7BA8`, proving
mode `$02` bypasses the settings tilemap,
two roster selections, caller/matcher ownership, saved spot `$74/$88`, the
exact tile-$76 X, a P2 5-to-4 letter decrement, and command `$07` APU writes.
Native mode 2 uses the same court, player animation, launch, flight, and rim
paths as the cartridge, plus the exact 50-pair `$6DB7` CPU spot table. The
same trace now proves `$22C3` clears the court's two colon placeholders,
`$0749/$7BA8/$06C0` supplies names and the ROM font, and `$1ECC` writes the
20/15/15/15-frame bend/deep/bend/rest net sequence.

The expanded Free Throw gameplay/presentation manifest is **32/32 (100.00%)**. It follows
`$0C8E->$17AA->$100F->$1942/$1986->$1CAA/$7C58->$7BE8->$1A31->$1C61`
through configured attempts, results, and exit, and now includes the exact
`$2243/$1CBD`, `$1828/$1858`, and `$1884` graphics paths plus `$1C1D`'s
prior-frame OAM priority and the `$C12B=$2D` made-ball gravity hold. Bank-2
`$4000:$4014-$401D->$4034` proves the one-player/no-VS selector, `$2243`
copies fixed `$22A9` to OBJ tile `$7F`, `$1A25` moves it through OAM
`$C098/$C099`, and `$0C8E` clears the title/menu music command at `$DD73`.
The `$1A31` outcome audit now also includes `$1A7E->$1AA6`'s exact
`$1AAD` X columns and `$1AB0/$1AB8/$1ABE` rating-dependent clean-make lists,
plus `$1BBD`'s smaller center-rim target override.

The separate One-on-One behavior manifest is **50/50 (100.00%)**. Its fixed requirements are now complete, including steals, moving defensive jumps, post-contact recovery that preserves action/direction/facing, and the ROM's explicit initial-flight lock/no-separate-goaltending behavior. This focused result does not promote incomplete whole-project milestones or candidate whole-routine mappings.

The expanded One-on-One shooting/presentation audit is **22/22 (100.00%)**,
up from **12/22 (54.55%)** when the former contact-only denominator was
corrected. The path now couples `$6A8C`, `$6C4D/$7F37`, `$7C58`, `$7BE8`, and
`$1CED->$1E0E` to `$1F23/$2F88`, `$0C13/$2D08`, `$27C7/$27EA`, `$20F7`,
`$27CC`, and the next `$702D` inbound update.

The separate remaining-focus manifest is **50/50 (100.00%)**: exact RNG is **10/10**, animation/assets are **20/20**, and collision/contact recovery is **20/20**. Ghidra corrected two speculative requirements: the ROM has no contact-hit/recoil animation and no rebound-pickup action assignment. Verified absence and the actual charging/blocking, CPU hold/reroute, and asset/OAM paths receive credit instead.

The expanded One-on-One presentation/audio manifest is **60/60 (100.00%)**.
The new live-flow manifest is **18/18 (100.00%)**; it covers shot-gather
movement, complete run/shot records, decoded command `$0C`, CPU drive/gather
decisions, and the made-ball bounce through fade entry.
It now also covers `$347B` instrument transposition, `$702D/$6B34/$7F0A`
phase-two dunk presentation, `$C12B/$69F5` ball/net priority, identical
`$7FC7/$7FCB` held-shot rows, and `$C178->$067C` take-back violations. The
earlier 55 items cover `$2DD2->$21FA` selected-player palettes, shared gameplay body art,
`$7138` hoop-facing shots, live `$2B14/$2B88` steals, `$05A3/$0C49` fouls,
sound commands `$04/$05/$08/$09/$0A/$0C/$0D/$0E/$0F`, focused `$3014` program
`$0A/$0C/$05/$0B/$0D/$02/$11/$12/$07` extraction, corrected `$6F2A` placement,
post-score take-out, and defensive-rebound take-back. The
whole bank-0 `$3014` interpreter remains a candidate mapping because only the
eleven focused programs are converted; whole-engine waveform/music parity
is not claimed.

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
| Reviewed Ghidra functions | 145 functions | 85 in bank 0, 52 in bank 1, and 8 in bank 2; every seed creates and decompiles successfully. |
| Generated Ghidra C export | 158 functions | Reviewed functions plus reset/interrupt vectors and thunks. |
| Reviewed routine-to-C parity | 67 of 145 | 67 mappings have address-level C plus deterministic or emulator evidence; 38 candidates remain below whole-routine evidence threshold. |
| `mgbdis` `Call_*` labels | 246 | Candidate entry points; some may be false code. |
| `mgbdis` `Jump_*` labels | 116 | Branch targets, not necessarily standalone functions. |
| Unique direct `call` targets | 251 | Analysis queue, not a verified function denominator. |
| Rows in the former detailed matrix | 43 | The former summary claimed 108 without listing 108 mappings. |
| Former matrix identifiers found verbatim in assembly | 33 of 43 | Identifier existence still does not prove the stated meaning. |
| Headless Mesen trace scripts | 22 | Includes deterministic One-on-One, Free Throw, H-O-R-S-E, and Accuracy menu-to-gameplay traces; Accuracy proves one-player routing, positions, scoring, timer/result, and `$02` APU writes. |

### Reviewed Ghidra recovery

`tools/ghidra/setup_banked_rom.py` creates and verifies the bank overlays. `recover_banked_functions.py` then consumes `function_seeds.json`, creates stable names such as `rom_b01_69f5`, decompiles each reviewed seed, and writes `build/ghidra_function_inventory.json`. `tools/check_ghidra_functions.py` rejects missing functions, wrong bank spaces, empty bodies, decompiler failures, and invalid native mapping records.

The current conservative boundaries are:

- bank 0: 85 boot, RNG, One-on-One, Free Throw, H-O-R-S-E, Accuracy lifecycle/rules, score-presentation, asset-load, player-composition, steal, recovery, and roster-audio routines;
- bank 1: 52 functions in coherent code beginning at `$6945`, including the `$6A8C` animation dispatcher, `$6F2A` final dribble placement, and four recovered Accuracy HUD writers; generated labels before that region remain excluded as likely data false positives;
- bank 2: code at `$4000..$42A1`, followed by a visible data boundary at `$42A2`;
- bank 3: no reviewed functions yet; observed uses are asset-copy sources.

Bank 1 `$76A7` is a confirmed call target but remains deferred because Ghidra follows its indirect table into an oversized invalid function and times out. It receives no whole-function credit; the four Accuracy dispatch targets `$7749/$7765/$7790/$77A1` are separately boundary-reviewed, recovered, decompiled, ported, and live-traced.

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
| `$702D/$7170` player and CPU controllers | 11/25 | 44.00% | +4.00 points |
| Free Throw `$0C8E/$100F` gameplay | 12/25 | 48.00% | +4.00 points |
| H-O-R-S-E `$0CDF/$0D57` gameplay | 13/25 | 52.00% | +4.00 points |
| Accuracy `$0E51/$6CA2` gameplay | 14/25 | 56.00% | +4.00 points |

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
| Roster selection | Yes | Scoped audio behavior | Navigation command `$0F`/program `$07` and accepted-player command `$0E`/five-step program `$12` are live-traced, extracted, and integrated; data and screen art remain hardcoded/partial. |
| One-on-One | Yes | 50/50 gameplay; 50/50 remaining focus; 60/60 presentation/audio | Lifecycle, possession, shooting/dunk phase, roster palettes, hoop-facing shots and held-ball offsets, live steals, RNG, complete `$702D` shared input control, record-boundary movement, charging/blocking/take-back presentation/audio, complete `$7170` CPU decisions, defender landing, grounded OAM, score-ball/net priority, rim recovery, animation records, and dynamically extracted One-on-One art/OAM are covered. This is not a frame-perfect claim. |
| Free Throws | Yes | 32/32 gameplay/presentation | Single-player selection bypasses VS; `$0C8E` stops menu music; `$22A9/$1A25` supplies the moving reticle; aim, rating-based clean makes, rim/net score, attempts/results, exact close-up art, `$1C1D` priority, HUD, and results are playable. |
| H-O-R-S-E | Yes | 30/30 scoped; 28/45 strict shared-routine | Two-player roster selection, caller/matcher turns, 50 CPU spots, saved X, exact names/letter font, colon clearing, shared shooting/net timing, audio `$07`, winner, and exit are playable and traced. |
| Accuracy Shootout | Yes | 25/25 one-player scope; 8/11 strict dedicated routines | Single-player selector, exact 50 positions, custom editor, marker approach, shared shot/net, exact court-panel clocks/scores, attempts/makes, timer/result, and command `$02` are traced and playable. |
| Tournament | Yes | Scoped gameplay flow | Four quarterfinals, two semifinals, final, champion lock, and exit are covered; presentation remains partial. |
| Two-player gameplay | No | No | Missing |
| Ball physics | Yes | Scoped One-on-One flight/contact | Exact 60 Hz 8.8 flight, live animation-record arc selection, record-coupled release height, 32/64-frame vectors, `$1CED` score/rim/backboard cells, `$1E77` bounce loss, outer/back-court response, and `$1F4D` stop are deterministic; other modes remain incomplete. |
| Player collision and possession rules | Partial | Focused One-on-One subset | In addition to recovery and steals, `$6A8C->$6E3C` blocking, `$2C50/$2CCA/$0AC5` charging/blocking, exact 25-count persistence, protected-shot clear, and no-recoil behavior are verified and live-integrated. |
| CPU AI | Yes | Verified `$7170-$761A` controller | Mode dispatch/gates, `$FFD2/$FFD3` synthesis, offense routes, defense offsets, loose-ball chase, profile/skill decisions, close-drive/special timers, `$75CD` contact behavior, `$74BB` 8/4/1 hysteresis, arrival/gather, and record-gated release are native and traced. |
| Player animation selection | Yes | Complete focused One-on-One path | `$782E/$6A8C/$6C60` records, `$6B72` movement callbacks, idle/steal/jump/shot families, and the verified absence of contact-hit/rebound-pickup actions are covered. |
| Court/menu/player rendering | Yes | One-on-One, Free Throw, and Horse scoped | Horse reuses the One-on-One court/player/ball/net streams, clears `$22C3`'s two colon cells, draws `$0749` names and `$7BA8/$06C0` letters from the extracted ROM font, and uses exact tile-$76 X and `$1ECC` net phases. Free Throw keeps its separate close-up art. |
| ROM asset extraction | Yes | One-on-One plus Free Throw scoped | Exact One-on-One sources plus Free Throw `$640F/$6EF1/$708E/$7F69`, pose/net maps, OBJ maps, `$050F` wrap behavior, endpoint validation, and runtime loading are implemented. |
| PCM output/mixing | Yes | Not applicable to original implementation | Native platform layer |
| ROM music/SFX sequencing | Partial | Event-level multi-mode subset | Eleven focused commands now include Accuracy `$02`, Horse `$07`, Free Throw `$08/$0A`, and One-on-One/selector cues. The complete `$3014` music/sequencer engine is not ported. |
| Emulator/state/frame parity suite | No | No | Missing |

No whole subsystem should currently be labeled 100% ROM-equivalent; the
50/50 figure is explicitly a fixed, focused One-on-One behavior manifest.

## Missing-Work Matrix

| Priority | Area | Required work for verified coverage |
|---|---|---|
| P0 | Four-bank Ghidra function recovery | 145 stable reviewed functions are recovered; finish remaining bank 0/1 boundaries, resolve the whole `$76A7` dispatcher, review bank 3, and export call graphs/memory references. |
| P0 | Coverage tooling | Milestone and routine inventories exist; add deterministic trace/test evidence before promoting any candidate mapping to verified. |
| Complete | Settings | Session persistence, ROM value cycles, and downstream consumption are verified; Accuracy mode parity remains tracked separately. |
| Complete | One-on-One scoped rules | 50/50 gameplay requirements and 50/50 remaining-focus requirements are verified, including collision/contact rules and user-ROM-derived presentation. Other modes and frame-perfect synchronization remain separate work. |
| Complete | Free Throws scoped parity | 32/32 single-player selection, music stop, moving reticle, aim, rating-based clean makes, rim/net scoring, attempts/results, `$08/$0A` audio, mode-specific assets, and `$1C1D` priority are verified. |
| Complete | Tournament | Seven-match winner propagation, bracket rounds, champion lock, and title return are verified; presentation remains outside this gameplay milestone. |
| Complete | H-O-R-S-E scoped parity | 30/30 called-shot, matching, letter, CPU/human, shared shot, exact HUD/font/net presentation, audio, winner, and exit requirements are verified; whole shared-routine coverage is 62.22%. |
| Complete | Accuracy Shootout one-player scope | `$4000/$4034` selection, `$0E51` lifecycle, `$6CA2/$6D57` positions, `$7AFD` approach, `$76A7` HUD targets, scoring, timer/result, shared assets/net, and `$02` audio are traced, ported, and tested. |
| P1 | Two-player game logic | Preserve the 1P/2P selection and add a second native input stream and two-human state flow. Serial hardware transport remains excluded. |
| P1 | Physics | One-on-One launch/contact/bounce, `$FFD7` 2/3-point selection, and `$6A8C` timing-to-launch-index logic are covered; extend equivalent evidence to other modes. |
| Complete | `$702D/$7170` controllers | Shared human/CPU player input, full CPU decision state, synthetic input handoff, direction hysteresis, contact response, mode dispatch, and record-gated shooting are verified. |
| Complete | Focused One-on-One animation/assets | Directional records, direct movement, verified no-contact-hit/no-pickup-action behavior, eight-phase ball/shadow OAM, player composition, and court/player/ball extraction are covered. |
| P1 | Audio sequencing | Recover the bank-0 audio command interpreter, song/SFX tables, channel timing, pitch, envelope, and event mappings. |
| P1 | Asset pipeline | One-on-One court/player/ball extraction is complete; portraits, logos, other-mode animation art, roster records, and audio data remain. |
| Complete | One-on-One runtime asset boundary | One-on-One derived art is removed from tracked headers and loaded from a user-built pack; other screens have separate legacy assets. |
| P1 | Parity tests | Input, defense, RNG, movement/contact, charging/blocking, and asset/OAM traces exist; add broader whole-game WRAM/native-state checkpoints if frame-perfect work resumes. |

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

The manifest now exists, so report its verified subset explicitly: **67/145 routine mappings verified**. Do not use 145 as a whole-ROM function denominator until the remaining code/data review is complete. Accuracy behavior is checked with `python tools/check_accuracy_coverage.py`; Horse, Free Throw, and One-on-One retain their separate scoped tools.
