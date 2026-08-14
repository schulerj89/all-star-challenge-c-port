# NBA All-Star Challenge (Game Boy to Native C Port)

A high-performance, native C port of **NBA All-Star Challenge** (Game Boy, 1991/1992, Beam Software / LJN).

This project decompiles and ports the game logic directly into standard C99 with a clean-room ROM asset extraction pipeline, software rasterizer, APU audio synthesizer, and multi-mode gameplay engine.

---

## Features

- **Native C Implementation**: Pure C99 game engine, not an emulator wrapper.
- **ROM Asset Extraction**: Automatically extracts graphics, tilemaps, fonts, rosters, and animation frames from an original Game Boy ROM into a single `.assetpack` binary file.
- **Game Modes Supported**:
  - One-on-One
  - Three-Point Shootout
  - Free Throw Competition
  - H-O-R-S-E
  - Tournament Bracket
- **Display Options**: Authentic 160×144 resolution with integer scaling (1x, 2x, 3x, 4x) and multiple palette styles (Original DMG Green, Game Boy Pocket Grayscale, Modern).
- **Automated Verification**: Comprehensive unit test suites and headless regression testing.

---

## Building the Port

### Windows (MSVC)
```powershell
.\build.ps1
```
This produces two binaries in `build/`:
- `allstar_port.exe` — Console CLI test harness, ROM validator, and asset packer.
- `allstar_port_game.exe` — Win32 GUI game executable.

### Building Asset Pack
```powershell
.\build\allstar_port.exe --build-assetpack path\to\nba_allstar_challenge.gb build\allstar.assetpack
```

### Running the Game
```powershell
.\build\allstar_port_game.exe --play
```

---

## Reverse Engineering Tooling

- **Ghidra + GhidraBoy**: Decompilation and disassembly of the Sharp SM83 core.
- **Ghidra MCP Server**: Model Context Protocol bridge allowing AI agents to query functions and symbol tables.
- **mgbdis**: Symbolic disassembly generation into RGBDS format.

See [PORTING.md](PORTING.md) and [AGENTS.md](AGENTS.md) for full engineering specifications.
