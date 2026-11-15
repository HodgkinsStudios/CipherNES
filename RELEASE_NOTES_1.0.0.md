# CipherNES 1.0.0 — release candidate notes

**HodgkinsStudios** · NES/Famicom emulation · Linux, Windows and macOS source targets.

This document is a **draft for final release review**, not a publication announcement. Review and validate all artifacts produced from the exact approved commit before tagging or publishing.

## Included functionality

- Native C++ emulation core; separate platform frontends and a public C-compatible API.
- Cartridge loading (iNES/NES 2.0, supported ZIP containers), user-supplied FDS BIOS support, battery-backed saves and save states.
- Keyboard/gamepad mapping, multiple fullscreen modes, rewind, fast-forward, Game Genie, and optional TAS/video capture.
- Headless verification utilities and maintainable build/packaging tools.

## Known validation results

- AccuracyCoin: **147 of 147 displayed PASS** on Ubuntu in the most recently confirmed full run.
- External NTSC ROM ledger: **197 PASS / 7 FAIL / 70 UNVERIFIED**. Some passing entries are limited functional demonstrations; see `docs/TESTING.md` before interpreting these scores.
- Synthetic mapper audit: **156 mapper IDs** recognized and **7,156 hardware-register assertions** passed; recognition is not complete accuracy certification.
- Host audio, timeline and focused save-state/mapper tests have passed on Ubuntu.
- Windows, macOS and release package builds require validation from the final release source commit.

## Disclosed limitations

- Edge-case PPU sprite/scanline/NMI synchronization tests remain unresolved.
- Certain original mapper/homebrew ROMs have known fixture or compatibility concerns; raw RGB presentation does not simulate all composite NTSC effects.
- PAL operation, rare mapper boards, analog audio fidelity, some fine MMC5 behavior and all platform frontends are not fully certified.
- Save states are versioned; compatibility with files produced by older public binaries is not guaranteed.
- No commercial ROMs, third-party test ROMs, FDS BIOS or user data are included.

See [testing](docs/TESTING.md), [build instructions](docs/BUILDING.md), [release checklist](docs/RELEASING.md), [license](LICENSE) and [notices](THIRD_PARTY_NOTICES.md).
