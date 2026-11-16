# Changelog

This project uses semantic versions for the source and release artifacts. A GitHub commit does not imply that a corresponding release has been published.

## 1.0.2 — branding and cross-platform packaging update (2026-11-15)

- Replaced the original project mark with the updated CipherNES NES-themed artwork at the top of the GitHub README and in the in-app About screens.
- Updated window and application branding assets for Linux, Windows and macOS: PNG artwork, embedded icon pixels, Windows ICO and macOS ICNS.
- Updated AppImage and Flatpak desktop icons and packaging paths; retained their separate package integrations.
- Set the project, core version strings, native About panels, metadata, release notes and artifact names to 1.0.2.
- Kept the existing shared emulator core, API/state format, mapper implementation and documented hardware-accuracy results unchanged; this is a branding and maintenance release.
- Automated builds cover Ubuntu x86_64, Windows x86_64, macOS Intel/Apple Silicon, Debian, Flatpak and AppImage; hosted CI does not substitute for full native GUI/audio/gamepad testing.

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
