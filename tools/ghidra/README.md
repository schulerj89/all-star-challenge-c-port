# Ghidra Tooling for NBA All-Star Challenge

This directory contains the bank-aware headless Ghidra recovery/export scripts and an optional HTTP/MCP bridge helper for reverse engineering the Game Boy ROM.

The current setup maps and verifies all four banks correctly and validates 100 conservatively reviewed functions. It is **not yet a complete four-bank gameplay decompilation**.

## Verified Cartridge Layout

`NBA All-Star Challenge (USA, Europe).gb` is a 64 KiB MBC1 image with four 16 KiB banks and no cartridge RAM.

| Physical bank | ROM file offsets | Game Boy CPU mapping |
|---|---|---|
| 0 | `0x0000..0x3FFF` | `$0000..$3FFF` fixed window |
| 1 | `0x4000..0x7FFF` | `$4000..$7FFF` switchable window |
| 2 | `0x8000..0xBFFF` | `$4000..$7FFF` switchable window |
| 3 | `0xC000..0xFFFF` | `$4000..$7FFF` switchable window |

Do not import physical banks 2 and 3 as CPU addresses `$8000..$FFFF`. A correct Ghidra project needs overlay spaces or an equivalent bank-aware layout for banks 1–3.

## Prerequisites

1. Ghidra 10.x or 11.x.
2. A compatible SM83/LR35902 processor module such as GhidraBoy.
3. Java/JDK supported by the selected Ghidra version.
4. Optional: a compatible Ghidra MCP/HTTP extension if interactive symbol and decompiler queries are needed.

## Current Headless Script

Run:

```powershell
.\tools\ghidra\run_ghidra_decomp.ps1
```

The script copies the user ROM into the ignored `build/` directory, invokes `analyzeHeadless`, and writes the decompiler output to:

```text
tools/decomp/ghidra_decompiled.c
```

It also writes and validates:

- `build/ghidra_bank_inventory.json` for source offsets, CPU windows, sizes, overlay flags, and byte digests;
- `build/ghidra_function_inventory.json` for reviewed bank-aware functions, decompiler status, and explicit native-counterpart status.

### Current result and limitations

- `setup_banked_rom.py` constructs and byte-verifies overlays for physical banks 1–3.
- `recover_banked_functions.py` creates and decompiles 100 checked-in seeds: bank 0 = 45, bank 1 = 47, bank 2 = 8.
- `decompile_all.py` no longer blindly sweeps unknown bytes as code; it exports reviewed functions plus standard vectors/thunks (113 functions in the current run).
- Stable names preserve physical bank identity, for example `rom_b02_4000`.
- Bank 1 `$76A7` is deferred because its unresolved control flow currently causes a decompiler timeout.
- Bank 3 has no reviewed function seeds; observed references currently use it as an asset-copy source.
- WRAM/HRAM fields are still unnamed, call graphs/memory references are not exported, and no complete gameplay subsystem has parity evidence.

The generated file must therefore be treated as preliminary evidence, not as a coverage denominator.

## Four-Bank Setup and Remaining Analysis

The headless setup maps bank 0 and creates byte-verified overlays for physical banks 1–3. Function recovery still needs to:

1. Preserve bank identity in every function name and cross-reference.
2. Expand the reviewed seed set with confirmed direct-call targets and indirect dispatch targets.
3. Use dynamic traces and `mgbdis` labels to find entry points, while independently verifying boundaries.
4. Mark tile, text, pointer, roster, animation, and audio regions as data before decompiling.
5. Extend the function/native-status inventory with call graphs, symbols, and memory references.

`mgbdis` currently reports 246 `Call_*` labels, 116 `Jump_*` labels, and 251 unique direct call targets across the four banks. These are candidates only: without a code/data map, generated assembly can interpret data as instructions.

## Known Anchors

| Address | Current interpretation | Confidence |
|---|---|---|
| Bank 0 `$0100` | Cartridge entry thunk | High |
| Bank 0 `$0150` | Main initialization | High |
| Bank 0 `$2639` | Joypad register polling | High |
| Bank 0 `$2729` | VBlank handler | High |
| Bank 0 `$1FFA` | Skill level to `8/4/1`-frame update delay | High |
| Bank 0 `$20D0` | Settings defaults | High |
| Bank 0 `$22EF` | Mode-specific settings editor | High |
| Bank 0 approximately `$3014..$36DB` | Multi-channel audio command/sequencing region | High for audio ownership; individual routines still need names |
| Bank 1 `$6A8C` / `$6C59` / `$6C60` | Player animation record dispatcher, selector, and 24-action pointer table | High; record engine verified, movement side effects partial |
| Bank 1 `$6CA2` | Accuracy computer/new-position selection | High for selector; full mode remains incomplete |
| Bank 1 approximately `$69F5..$7FFF` | Player/gameplay/rendering state region | Medium; routine-level meanings remain under audit |

Generated labels such as `Call_001_447c` are not automatically trustworthy. Confirm that an address is code before assigning a semantic name.

## MCP Bridge Helper

`ghidra_mcp_bridge.py` is a small client for a separately running compatible server. It exposes helpers for:

- decompiling a function;
- listing symbols;
- renaming a symbol.

The helper does not start Ghidra, install an MCP extension, or guarantee that a particular server implements those HTTP routes. Connection failures are returned as JSON errors.

## Coverage Standard

Do not mark a ROM routine implemented because a similarly named C function or comment exists. A verified mapping needs:

- a confirmed physical bank and CPU address;
- confirmed function boundaries and purpose;
- documented input and state dependencies;
- a native C counterpart;
- emulator trace, state comparison, or deterministic output comparison;
- an automated test for the stated behavior.

The current audited coverage and missing-work table are maintained in [`docs/GHIDRA_COVERAGE.md`](../../docs/GHIDRA_COVERAGE.md).
