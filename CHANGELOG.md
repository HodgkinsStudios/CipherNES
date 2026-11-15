# Changelog

This project uses semantic versions for the source and release artifacts. A GitHub commit does not imply that a corresponding release has been published.

## 1.0.1 — release-preparation candidate (2026-11-15)

- Professional GitHub README with the measured AccuracyCoin 147/147 final-screen capture.
- Consolidated accuracy report including 197 PASS / 7 FAIL / 70 UNVERIFIED NTSC corpus verdicts, scope restrictions and upstream test-suite acknowledgements.
- Applied consistent v1.0.1 version metadata to package naming, CI candidate workflow and native core version strings.
- Retained prior PPU forced-blanking palette and APU triangle-phase hardware corrections from the authoritative development source.
- Final public-source cleanup removes `TEST_ROMS/`, `tests/`, `tools/`, and test-only CI utilities; historical accuracy evidence remains credited and archived separately.
- Verified identical shared core compilation lists for Linux, Windows and macOS; retained production build and packaging automation.
- No new game-specific hacks or new accuracy verdicts claimed in this documentation/version pass.

## 1.0.0 — initial source release

- NES/Famicom shared emulation core and desktop frontends for Linux, Windows and macOS.
- CPU, PPU, APU, FDS and cartridge mapper emulation, save states, rewind, configurable controls and optional TAS/capture support.
- Headless regression checks and cross-platform packaging workflows.
