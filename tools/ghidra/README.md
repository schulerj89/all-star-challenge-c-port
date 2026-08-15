# Ghidra Tooling for NBA All-Star Challenge

This directory contains the preliminary headless Ghidra export script and an optional HTTP/MCP bridge helper for reverse engineering the Game Boy ROM.

The current setup maps and verifies all four banks correctly. The decompiler export remains useful mainly for boot and interrupt discovery and is **not yet a complete four-bank gameplay decompilation**.

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

It also writes `build/ghidra_bank_inventory.json` and validates that all four banks have the correct source offsets, CPU windows, sizes, overlay flags, and byte digests.

### Known limitations

- `setup_banked_rom.py` constructs and byte-verifies overlays for physical banks 1–3.
- `decompile_all.py` still sweeps only the default `$0000..$7FFF` space and does not recover overlay functions.
- It explicitly creates only boot/vector functions; auto-analysis currently discovers very few additional functions.
- The audited export contains 16 functions, mostly vectors, thunks, boot/init, serial handling, and VBlank.
- Function names are mostly generic and WRAM/HRAM fields are unnamed.
- No complete gameplay subsystem has been recovered by this script.

The generated file must therefore be treated as preliminary evidence, not as a coverage denominator.

## Four-Bank Setup and Remaining Analysis

The headless setup now maps bank 0 and creates byte-verified overlays for physical banks 1–3. Function recovery still needs to:

1. Preserve bank identity in every function name and cross-reference.
2. Create functions at confirmed direct-call targets and indirect dispatch targets.
3. Use dynamic traces and `mgbdis` labels to find entry points, while independently verifying boundaries.
4. Mark tile, text, pointer, roster, animation, and audio regions as data before decompiling.
5. Export functions, call graphs, symbols, and memory references in a machine-readable manifest.

`mgbdis` currently reports 246 `Call_*` labels, 116 `Jump_*` labels, and 251 unique direct call targets across the four banks. These are candidates only: without a code/data map, generated assembly can interpret data as instructions.

## Known Anchors

| Address | Current interpretation | Confidence |
|---|---|---|
| Bank 0 `$0100` | Cartridge entry thunk | High |
| Bank 0 `$0150` | Main initialization | High |
| Bank 0 `$2639` | Joypad register polling | High |
| Bank 0 `$2729` | VBlank handler | High |
| Bank 0 approximately `$3014..$36DB` | Multi-channel audio command/sequencing region | High for audio ownership; individual routines still need names |
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
