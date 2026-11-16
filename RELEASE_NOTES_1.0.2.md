# CipherNES v1.0.2 — updated branding release

**HodgkinsStudios** · NES / Famicom emulator · Linux, Windows and macOS

v1.0.2 updates the official application identity and platform release packages. It is a branding-focused maintenance release following v1.0.1, not a new emulation-accuracy milestone.

## What's new

- New CipherNES artwork shown at the top of the GitHub README.
- The same artwork now appears in the **Help → About** panel and as the SDL application window icon in the native frontends on Linux, Windows and macOS.
- Updated application icons and branded resources for Windows (`.ico`), macOS (`.icns`), Ubuntu/Linux (`.png`), Flatpak and AppImage.
- Updated AppImage/Flatpak desktop icon packaging, release version strings, package metadata and names across the supported builds.
- Original project icon removed from the source/packaging paths where it was superseded.

## Supported release build targets

| Target | Distribution |
| --- | --- |
| Ubuntu / Linux x86_64 | Native ZIP |
| Ubuntu / Debian amd64 | `.deb` |
| Linux x86_64 | AppImage |
| Linux x86_64 | Flatpak |
| Windows x86_64 | Native ZIP |
| macOS Intel x86_64 | Application ZIP and single-file executable |
| macOS Apple Silicon arm64 | Application ZIP and single-file executable |
| Source | ROM-free source ZIP |

The GitHub Actions workflows compile and perform supported CLI/smoke/package checks. Completion of hosted CI is **not** proof of manual GUI, audio, gamepad, filesystem-dialog, runtime-signing or physical-device functionality. macOS artifacts are not Apple notarized.

## Accuracy and limitations

The established Ubuntu **AccuracyCoin 147/147 PASS** result and its authentic screenshot remain documented in the README. The recorded NTSC test-ROM classification remains **197 PASS / 7 FAIL / 70 UNVERIFIED** (274 entries). Smoke testing at 600 frames was **274/274 executions**, **not** 274 hardware-test passes. Mapper synthetic checks covered **156 registered mapper IDs / 7,156 assertions**. These are carried-forward development results; the full external test corpus was not rerun for the branding change. No emulator accuracy improvement or new test passes are claimed for this release.

For methodology, failures and test-suite credits, see [docs/TESTING.md](docs/TESTING.md). For known deployment limitations and release verification, see [docs/RELEASING.md](docs/RELEASING.md).

## Packaging and distribution

- Build all platform outputs from the **same approved v1.0.2 commit** and verify the bundle architecture and release version.
- Check that each package includes the **updated application artwork**, that **Help → About** displays it, and that the OS application/window icon matches where supported.
- Check SHA-256 hashes for all deliverables before adding them to a release. Do not recycle v1.0.1 binaries, since they predate the branding change.
- Only include ROM-free source and redistributable program assets. No commercial game ROMs, test-ROM binaries, NES/FDS firmware, saves, or private checkpoints are bundled.

**Release status:** prepared source candidate; final publication is a separate explicit action after verification.
