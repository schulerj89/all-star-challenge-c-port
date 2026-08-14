# Ghidra & Ghidra MCP Tooling for NBA All-Star Challenge (GB)

This directory contains configuration, scripts, and documentation for reverse engineering the Game Boy ROM using Ghidra and connecting AI agents via the Model Context Protocol (MCP).

---

## 1. Prerequisites

1. **Ghidra (v10.x or v11.x)**: [https://ghidra-sre.org](https://ghidra-sre.org)
2. **GhidraBoy Extension**:
   - Download the pre-built extension zip from [GhidraBoy Releases](https://github.com/Gekkio/GhidraBoy/releases).
   - In Ghidra: `File -> Install Extensions -> '+' -> Select GhidraBoy zip -> Restart Ghidra`.
   - GhidraBoy provides:
     - Sharp SM83 / LR35902 CPU processor model.
     - Game Boy memory maps (`0x0000..0xFFFF`).
     - Standard DMG I/O register symbols (`0xFF00..0xFF4B`).
3. **Ghidra MCP Server**:
   - Recommended: [`ghidra-mcp`](https://github.com/bethington/ghidra-mcp) or [`GhidrAssistMCP`](https://github.com/SymGraph/GhidrAssistMCP).
   - Enables AI agents to call Ghidra tools:
     - `decompile_function(addr_or_name)`
     - `get_function_signature(name)`
     - `rename_symbol(addr, new_name)`
     - `get_cross_references(addr)`
     - `get_data_structure(addr)`

---

## 2. Importing NBA All-Star Challenge ROM into Ghidra

1. Create a new Ghidra project: `File -> New Project -> Non-Shared Project -> NBA_AllStar_GB`.
2. Import the ROM: `File -> Import File -> Select nba_all_star_challenge.gb`.
3. Ghidra will detect the format as **Game Boy ROM** (via GhidraBoy) with processor **SM83 (LR35902)**.
4. Memory Map:
   - `ROM0`: `0x0000..0x3FFF` (16 KB Bank 0)
   - `ROM1`: `0x4000..0x7FFF` (16 KB Bank 1 - Fixed MBC0)
   - `VRAM`: `0x8000..0x9FFF` (8 KB Video RAM)
   - `WRAM`: `0xC000..0xDFFF` (8 KB Work RAM)
   - `HRAM`: `0xFF80..0xFFFE` (127 Bytes High RAM)
   - `IO`: `0xFF00..0xFF7F` (Hardware Registers)
5. Run **Auto Analysis** with default analyzers enabled.

---

## 3. Ghidra MCP Bridge

The bridge script `ghidra_mcp_bridge.py` allows command-line and agent queries to Ghidra's decompiler backend.
