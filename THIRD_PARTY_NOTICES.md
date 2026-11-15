# Third-Party Notices

CipherNES is distributed under the MIT License in `LICENSE`.

CipherNES was developed and validated using public NES hardware documentation plus independent test and reference projects. The public source repository deliberately includes no `TEST_ROMS/` directory, independent test-ROM binaries, test runners or diagnostic development tools; those remain external to release sources. Their individual licenses and redistribution rights have not been confirmed, and they are **not** covered by the CipherNES MIT license. Do not make the ROM-inclusive tree public until permissions have been reviewed.

## AccuracyCoin / TriCNES

- AccuracyCoin, maintained by Chris "100th_Coin" Siebert, is used as an independent NES accuracy test suite.
- TriCNES, also by Chris Siebert, is MIT-licensed and was consulted as a timing reference for difficult PPU/APU/DMA behavior.
- Project: `https://github.com/100thCoin/AccuracyCoin`
- Reference emulator: `https://github.com/100thCoin/TriCNES`

## AprNes

AprNes was consulted as an additional behavioral reference while resolving DMC/DMA edge cases. Its repository is distributed under the WTFPL.

- Project: `https://github.com/erspicu/AprNes`

## NESdev

The NESdev Wiki and community documentation were used as hardware references for the Ricoh 2A03/2C02, cartridge mappers, Famicom Disk System, and related behavior.

- Documentation: `https://www.nesdev.org/wiki/Nesdev_Wiki`

Third-party names and trademarks belong to their respective owners.

## Development test ROMs (optional developer fixtures)

For compatibility checks, the project used the independent NES test-ROM suite
maintained at https://github.com/christopherpow/nes-test-roms .
All ROM binaries, authorship and any licenses remain with their respective
authors. Possession of third-party test images is not an authorization to
redistribute them publicly. The normal ROM-free source and release ZIPs do
not bundle test ROMs or an FDS BIOS.
