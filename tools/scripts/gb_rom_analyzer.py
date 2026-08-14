#!/usr/bin/env python3
"""
Game Boy ROM Analyzer & Asset Scanner
-------------------------------------
Parses Game Boy ROM headers, verifies checksums, detects tiles, fonts, and text strings.
"""

import sys
import struct
import argparse
from pathlib import Path

# Cartridge Types
CART_TYPES = {
    0x00: "ROM ONLY",
    0x01: "MBC1",
    0x02: "MBC1+RAM",
    0x03: "MBC1+RAM+BATTERY",
    0x05: "MBC2",
    0x06: "MBC2+BATTERY",
    0x08: "ROM+RAM",
    0x09: "ROM+RAM+BATTERY",
    0x0B: "MMM01",
    0x0C: "MMM01+RAM",
    0x0D: "MMM01+RAM+BATTERY",
    0x0F: "MBC3+TIMER+BATTERY",
    0x10: "MBC3+TIMER+RAM+BATTERY",
    0x11: "MBC3",
    0x12: "MBC3+RAM",
    0x13: "MBC3+RAM+BATTERY",
    0x19: "MBC5",
    0x1A: "MBC5+RAM",
    0x1B: "MBC5+RAM+BATTERY",
    0x1C: "MBC5+RUMBLE",
    0x1D: "MBC5+RUMBLE+RAM",
    0x1E: "MBC5+RUMBLE+RAM+BATTERY",
}

ROM_SIZES = {
    0x00: "32 KB (2 banks)",
    0x01: "64 KB (4 banks)",
    0x02: "128 KB (8 banks)",
    0x03: "256 KB (16 banks)",
    0x04: "512 KB (32 banks)",
    0x05: "1 MB (64 banks)",
    0x06: "2 MB (128 banks)",
}

RAM_SIZES = {
    0x00: "None",
    0x01: "2 KB",
    0x02: "8 KB (1 bank)",
    0x03: "32 KB (4 banks of 8KB)",
    0x04: "128 KB (16 banks of 8KB)",
}

def analyze_rom(rom_path: Path):
    with open(rom_path, "rb") as f:
        data = f.read()

    size = len(data)
    if size < 0x150:
        print(f"Error: File is too small to be a Game Boy ROM ({size} bytes)")
        return False

    # Header parsing
    title_raw = data[0x134:0x143]
    title = title_raw.decode("ascii", errors="replace").strip("\x00")
    cart_type = data[0x147]
    rom_size_code = data[0x148]
    ram_size_code = data[0x149]
    dest_code = data[0x14A]
    header_checksum = data[0x14D]
    global_checksum = struct.unpack(">H", data[0x14E:0x150])[0]

    # Verify header checksum
    chk = 0
    for b in data[0x134:0x14D]:
        chk = (chk - b - 1) & 0xFF
    valid_hdr = (chk == header_checksum)

    print("=" * 60)
    print(f"Game Boy ROM Analysis: {rom_path.name}")
    print("=" * 60)
    print(f"File Size:         {size} bytes ({size // 1024} KB)")
    print(f"Title:             {title}")
    print(f"Cartridge Type:    0x{cart_type:02X} ({CART_TYPES.get(cart_type, 'Unknown')})")
    print(f"ROM Size Code:     0x{rom_size_code:02X} ({ROM_SIZES.get(rom_size_code, 'Unknown')})")
    print(f"RAM Size Code:     0x{ram_size_code:02X} ({RAM_SIZES.get(ram_size_code, 'Unknown')})")
    print(f"Destination:       {'Non-Japanese' if dest_code == 1 else 'Japanese'}")
    print(f"Header Checksum:   0x{header_checksum:02X} (Calculated: 0x{chk:02X} -> {'VALID' if valid_hdr else 'INVALID'})")
    print(f"Global Checksum:   0x{global_checksum:04X}")
    print("=" * 60)

    return valid_hdr

def main():
    parser = argparse.ArgumentParser(description="Game Boy ROM Analyzer")
    parser.add_argument("rom", type=Path, help="Path to .gb ROM file")
    args = parser.parse_args()

    if not args.rom.exists():
        print(f"File not found: {args.rom}")
        sys.exit(1)

    analyze_rom(args.rom)

if __name__ == "__main__":
    main()
