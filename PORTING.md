# NBA All-Star Challenge - Native C Porting Guide

This project is a native C port of **NBA All-Star Challenge** (Game Boy, 1991/1992, developed by Beam Software and published by LJN).
The goal is **not** to wrap an emulator, replay emulator logs at runtime, or make the game depend on a decompilation checkout. The final runtime owns game concepts in native C and loads its assets from ROM-derived asset packs.

---

## 1. Core Principles

1. **Native Game Concepts First**:
   - Runtime code uses native game concepts: scenes, screens, players, rosters, court coordinates, ball trajectories, shot timing curves, AI decision trees, tile layers, palettes, and sound events.
   - Game Boy hardware storage concepts (ROM banks, VRAM addresses, OAM slots, I/O registers `0xFF00..0xFF7F`) belong strictly in low-level importers, reference decoders, and reverse engineering tools.

2. **ROM-Only Asset Pipeline**:
   - The primary import path operates strictly on the original user-supplied ROM:
     ```powershell
     .\build\allstar_port.exe --build-assetpack <ROM_PATH.gb> build\allstar.assetpack
     ```
   - No copyrighted ROM bytes, extracted tiles, or proprietary binary tables are committed to version control.

3. **No Decompilation Runtime Dependency**:
   - Decompilation outputs (Ghidra C pseudocode, mgbdis disassembly, symbol maps) are read-only reference materials for reverse engineering. The native executable never depends on them at runtime.

4. **Modular Architecture**:
   - `allstar_game.c` orchestrates overall execution.
   - Scene logic, rendering, asset management, AI, and audio live in dedicated modules with clear, testable C APIs.

---

## 2. Game Boy (DMG-01) Target Specifications

| System Component | Game Boy Specification | Native C Port Implementation |
| :--- | :--- | :--- |
| **CPU** | Sharp SM83 (8-bit @ 4.194304 MHz) | Native C functions & state machines |
| **ROM Size / Mapper** | **32 KB / MBC0 (No Mapper)** | 32 KB flat memory buffer in importer |
| **Address Space** | `0x0000..0x7FFF` (Flat 32KB) | Direct offset indexing `0x0000..0x7FFF` |
| **Display Resolution** | 160 × 144 pixels @ 59.7275 Hz | 160 × 144 32-bit ARGB software framebuffer |
| **Color Palettes** | 4-shade grayscale (`00` White, `01` Light, `10` Dark, `11` Black) | Configurable RGBA palettes (DMG Green, Pocket B&W, Modern) |
| **Tile Format** | 2bpp planar tiles (16 bytes per 8×8 tile) | Decoded 8bpp / 32bpp indexed tile buffers |
| **OAM Sprites** | 40 sprites (4 bytes each: Y, X, Tile, Flags) | Native `AllStarSprite` struct & rendering queue |
| **Audio** | 4-channel APU (2 Square, 1 Wave, 1 Noise) | Native sound synthesizer & sound effect triggers |

---

## 3. Reverse Engineering Toolchain

### Ghidra + GhidraBoy + Ghidra MCP
1. **GhidraBoy Extension**: Enables SM83 CPU disassembly, P-code semantics, memory map preset (`0x0000..0xFFFF`), and DMG I/O register names.
2. **Ghidra MCP Server**: Enables AI agents to query decompiled functions, rename variables, extract jump tables, and trace references directly via MCP tools.
3. **Symbol Workflow**:
   - ROM entrypoint: `0x0100` (JP to initialization code).
   - Interrupt vectors: `0x0040` (VBlank), `0x0048` (LCD STAT), `0x0050` (Timer), `0x0058` (Serial), `0x0060` (Joypad).
   - WRAM layout: `0xC000..0xDFFF` (Main game state, player data, court physics).
   - HRAM layout: `0xFF80..0xFFFE` (Time-critical variables, OAM DMA routine).

### Static Disassembly with `mgbdis`
- Run `python tools/scripts/mgbdis_runner.py <ROM_PATH.gb>` to generate labeled RGBDS-compatible assembly.

---

## 4. ROM Bank Architecture & Reverse-Engineered Layout

From Ghidra decompilation and disassembly of `NBA All-Star Challenge (USA, Europe).gb`:

| Bank | Address Space | Contents & Decompiled Scope |
|---|---|---|
| **Bank 0** | `0x0000..0x3FFF` | **Core Engine & Mode Dispatcher (166 routines)**: Boot (`0x0150`), Mode Jump Table (`0x0267`), Joypad polling (`$FF00`), Ball physics (`0x100F`), Basket collision at `(80, 26)`, 1-on-1 mode (`0x0B80`), 3-Point Shootout (`0x0C8E`), Free Throw (`0x0CDF`), HORSE (`0x0E51`), Tournament (`0x0F2E`). |
| **Bank 1** | `0x4000..0x7FFF` | **Graphics & Animation Engine (65 routines)**: 2BPP planar tile decoder, character sprite frames, and VRAM copy routines. |
| **Bank 2** | `0x8000..0xBFFF` | **Audio Engine & Authentic 27-Player Database (14 routines)**: Audio sequencer and the complete 27-player NBA All-Star table at `0x8368` (Jordan, Bird, Barkley, Magic, Ewing, etc. with skin tone `0x90`/`0x91`, height, weight, PPG, and ratings). |
| **Bank 3** | `0xC000..0xFFFF` | **Tilemaps & Court Assets (6 routines)**: Half-court background, title logo, and menu layout tilemaps. |

---

## 5. Runtime & Architecture Design

### Frame Update Cycle
```c
void allstar_game_tick(AllStarGame *game, float dt) {
    allstar_controls_poll(game->controls);
    allstar_scene_update(game->active_scene, game, &game->controls->input, dt);
    allstar_renderer_clear(game->renderer, 0);
    allstar_scene_draw(game->active_scene, game, game->renderer);
    allstar_audio_update(&game->audio, dt);
}
```

### Game Modes
1. **One-on-One (`0x0B80`)**: Half-court game featuring authentic NBA legends, offensive drives, perimeter shooting, and defensive blocks/steals.
2. **Three-Point Shootout (`0x0C8E`)**: Five shooting racks around the 3-point arc with timed shooting gauges.
3. **Free Throw Competition (`0x0CDF`)**: Accuracy and timing gauge challenge.
4. **H-O-R-S-E (`0x0E51`)**: Precision shot matching challenge.
5. **Tournament (`0x0F2E`)**: 8-player bracket play leading to the All-Star championship.

---

## 6. Verification, Testing & Emulator Comparison

### Automated Tests
```powershell
.\build\allstar_port.exe --test-all
```
- `allstar_port.exe --rom-test <ROM_PATH>`: Validates ROM header and checksum.
- `allstar_port.exe --build-assetpack <ROM_PATH> build\allstar.assetpack`: Tests asset pipeline.
- `allstar_port.exe --test-roster`: Validates authentic 27-player attributes and stats.
- `allstar_port.exe --test-physics`: Tests ball trajectories and collision bounds.
- `allstar_port.exe --test-headless-frames`: Runs headless multi-scene render verification.

### 1:1 Side-by-Side Emulator Verification
Launch the original ROM in **mGBA** or **Mesen2** side-by-side with the native C port:
```powershell
.\tools\scripts\Launch-Emulator-Comparison.ps1 -Emulator mgba
```
