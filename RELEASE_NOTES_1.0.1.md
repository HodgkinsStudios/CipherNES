# CipherNES v1.0.1 — release preparation

**HodgkinsStudios** · Native NES/Famicom emulator · Linux, Windows and macOS source targets.

This document summarizes the **v1.0.1 release candidate source**. It is not a statement that a GitHub Release, tag, or platform binaries have already been published. Publication and platform sign-off are separate steps.

## Highlights

- Refreshed, focused GitHub README with a genuine **AccuracyCoin 147/147** screenshot and clear links to detailed validation documentation.
- Consolidated source-tree guidance for using, building, testing, architecture and release preparation.
- Carried forward the accuracy improvements already made during emulator development, including PPU forced-blanking palette behavior and APU triangle phase handling.
- Preserved the existing CPU, PPU, APU, cartridge mapper, FDS, save-state, rewind and native frontend functionality; no ROM-specific compatibility hacks were added during release preparation.
- Removed obsolete checkpoint source dumps, compiled binaries and non-redistributable third-party ROM/test binaries from the intended GitHub source tree.
- Aligned VERSION, project banners, package metadata and CI filename conventions to **1.0.1**.

## Recorded accuracy and reliability

| Test | Recorded result |
| --- | --- |
| AccuracyCoin | **147 / 147 PASS** on Ubuntu; actual result screenshot included |
| NTSC test-ROM verdicts | **197 PASS / 7 FAIL / 70 UNVERIFIED**, including limited functional demonstration passes |
| NTSC 600-frame smoke test | **274 / 274** executed and restored state (not hardware assertion passes) |
| Mapper regression | **156 mapper IDs; 7,156 passing assertions** |
| Targeted Mapper 33 / state replay | **44 / 44** and **48 / 48** respectively |

These are **carried-forward documented results**; the full NTSC corpus was not freshly rerun for this release-documentation change. See [Accuracy and testing](docs/TESTING.md) for failures, test origin credits and methodology.

## Known limitations and release gates

- Seven NTSC fixture entries remain failing, with 70 more unverified. Timing-sensitive raster and sprite checks, some original test fixtures, and analog composite-video behavior require further work or assessment.
- PAL accuracy, rare mapper variants and original-console audio fidelity are not fully certified.
- Windows/macOS native GUI, gamepad, display, audio and distribution packages require final validation from the tagged commit. Compiling a target is not a substitute for hardware and UI testing.
- No game ROMs, third-party test-ROM binaries, FDS BIOS firmware or personal save files are bundled.
- Save-state compatibility with older builds is not guaranteed without explicit testing.

## Release preparation

Use the scripts and steps in [docs/RELEASING.md](docs/RELEASING.md). The project is prepared for a **future** v1.0.1 release, but creating a GitHub commit does not create a published Release or a release tag.
