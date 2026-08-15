# Agent Notes & Engineering Protocol

These notes define standards and procedures for AI agents (Antigravity/Codex) working in this repository.

## 1. Golden Rules

- **Native C Port**: Do not create an emulator wrapper or log replayer. Implement native C data structures, state machines, math, rasterization, and audio.
- **Data Boundary**: NEVER commit ROM binaries, raw PRG/CHR dumps, extracted copyright sprite sheets, or private binary blobs.
- **Asset Pipeline**: Assets must be extracted dynamically from the user's ROM file using `--build-assetpack`.
- **Ghidra & MCP Integration**: Use the Ghidra MCP tools / scripts in `tools/ghidra` to analyze functions, query symbol tables, and decompile routines.
- **Evidence-Based Coverage**: Do not count comments, declarations, similarly named functions, or scene scaffolds as ROM-routine coverage. Mark behavior verified only when a trace or deterministic comparison supports it.
- **Keep Verification Green**: Ensure `.\build.ps1` builds cleanly without warnings and all verification tests pass.

## 2. Standard Build & Test Commands

```powershell
# Build both CLI test executable and Win32 game executable
.\build.ps1

# Run ROM validation
.\build\allstar_port.exe --rom-test <ROM_PATH>

# Build native asset pack from ROM
.\build\allstar_port.exe --build-assetpack <ROM_PATH> build\allstar.assetpack

# Run modular test suites
.\build\allstar_port.exe --test-roster
.\build\allstar_port.exe --test-physics
.\build\allstar_port.exe --test-headless-frames
.\build\allstar_port.exe --test-all
```

## 3. Subagent Workflow

1. **Explorer Subagents**: Use read-only subagents to search disassembly, analyze Ghidra decompilation, and map out RAM addresses.
2. **Worker Subagents**: Implement specific modular features (e.g. `scene_three_point.c`, `allstar_physics.c`, `allstar_audio.c`) with clean unit tests.
3. **Reviewer Subagents**: Inspect code quality, memory safety, and adherence to `PORTING.md`.

## 4. File Layout Conventions

- `include/`: All public header files. Headers must be self-contained and C99-compliant.
- `src/`: Core logic, CLI parsing, ROM loading, and asset packing.
- `src/scenes/`: Individual scene state machines (Intro, Menu, 1-on-1, 3-Point, Free Throw, HORSE).
- `src/gameplay/`: Physics, rules, and AI decision logic.
- `src/audio/`: Native audio output. The current implementation is a Win32 PCM mixer; ROM sequence interpretation remains planned work.
- `tools/`: Reverse engineering helpers, Ghidra MCP server bridge, and extraction scripts.
