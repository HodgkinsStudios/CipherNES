# CipherNES 1.0.0 — cleaned source release

**HodgkinsStudios** · NTSC NES/Famicom emulator · source prepared November 8, 2026.

This release is the cleaned, GitHub-ready presentation of the CP021 emulator implementation. The previous public `1.0.0`/`1.0.1` releases are historical; **do not replace an existing remote tag without an explicit release-management decision**. The source release version is intentionally set to `1.0.0` at the owner's request.

## Features

- Shared native emulation core, separate Ubuntu/Linux, Windows, and macOS frontends; keyboard/gamepad, native file loading, rewinding, save states, Game Genie, FDS with user-supplied BIOS, optional TAS and capture tools.
- Recognizes 156 mapper IDs in the mapper regression registry. This is *not* 156 fully verified board implementations.
- MMC5 ExRAM/banking/audio/split functions have focused coverage but some analog/scanline behavior remains unverified. ROM-specific hacks are not used as a substitute for hardware behavior.

## Recorded accuracy evidence

- AccuracyCoin full run: **147/147 displayed PASS** after a fresh combined run on this clean v1.0.0 Ubuntu build. Completed at frame 4,159; save-state roundtrip PASS. Windows/macOS require independent validation.
- Fresh synthetic mapper audit: **156/156 registered IDs; 7,156/7,156 assertions passed**.
- External ZIP: 289/289 valid ROMs had previously executed 600 frames and restored state; these are *not* 289 internal accuracy verdicts.
- 290-entry archival grading ledger at CP021: 175 documented internal passes, 0 confirmed fails, 1 visual-reference NMI sync mismatch (3 pixels), plus remaining non-graded/reference/PAL entries.

## Limitations

- NTSC is the supported timing target; PAL test results are not claimed.
- `nmi_sync/demo_ntsc.nes` shows a three-pixel offset against the supplied source's reference tolerance, and is **not** certified as passing.
- Game-specific and hardware-variant behavior may remain incomplete, including MMC5 analog audio and fine rendering timing.
- The current core state format is **v11**, and the core C API is **v11**. Do not assume compatibility with older public `1.0.0` or `1.0.1` save-state files.
- Windows/macOS builds and distributable packages require CI/device validation from this exact tree before publication.

The release ZIP does **not** contain ROMs, copyrighted FDS firmware, private screenshots, or compiled builds.
