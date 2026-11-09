#!/usr/bin/env python3
"""Created by Jacob Hodgkins. Generate a tiny original CI-only NES test program.

No commercial game data or copyrighted ROM assets are used. The program fills
RAM deterministically while reading the controller, then loops indefinitely.
"""
import pathlib
import sys


def main():
    if len(sys.argv) != 2:
        raise SystemExit('Usage: make_synthetic_rom.py OUTPUT.nes')
    path = pathlib.Path(sys.argv[1])
    path.parent.mkdir(parents=True, exist_ok=True)
    # iNES mapper 0, 16 KiB PRG, 8 KiB CHR, horizontal mirroring.
    header = b'NES\x1a' + bytes([1, 1]) + bytes(10)
    program = bytes([
        0x78,                   # SEI
        0xD8,                   # CLD
        0xA2, 0xFF,             # LDX #$FF
        0x9A,                   # TXS
        0xA9, 0x00,             # LDA #$00
        0x85, 0x00,             # STA $00
        # loop ($8009): strobe controller, read one bit, accumulate
        0xA9, 0x01,             # LDA #$01
        0x8D, 0x16, 0x40,       # STA $4016
        0xA9, 0x00,             # LDA #$00
        0x8D, 0x16, 0x40,       # STA $4016
        0xAD, 0x16, 0x40,       # LDA $4016
        0x45, 0x00,             # EOR $00
        0x85, 0x00,             # STA $00
        0x4C, 0x09, 0x80,       # JMP $8009
    ])
    prg = bytearray([0xEA] * 0x4000)
    prg[:len(program)] = program
    # Interrupt vectors at mirrored CPU addresses $FFFA-$FFFF.
    prg[-6:] = bytes([0x00, 0x80] * 3)
    path.write_bytes(header + prg + bytes(0x2000))
    print(f'Created original synthetic test ROM: {path}')


if __name__ == '__main__':
    main()
