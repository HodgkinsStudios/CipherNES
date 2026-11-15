# Release preparation checklist

CipherNES source version is defined by `VERSION`. **Do not publish releases, force tags, or overwrite an existing public release automatically.** GitHub preparation is not publication; v1.0.1 remains subject to platform QA. Final binaries must be built and tested from one explicitly approved commit.

## Before creating a candidate

1. Review `VERSION`, [`README.md`](../README.md), [`RELEASE_NOTES_1.0.1.md`](../RELEASE_NOTES_1.0.1.md), [`CHANGELOG.md`](../CHANGELOG.md), [`LICENSE`](../LICENSE) and [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).
2. Verify ROM-free source inventory and confirm no private saves, BIOS, games, test-ROM binaries, screenshots or compiled/debug artifacts ship in the source export.
3. Run all reproducible Ubuntu checks in [TESTING.md](TESTING.md), including source version checks and fresh AccuracyCoin grading with an authorized external ROM.
4. Build/test Linux native, Windows x86_64, macOS Intel and Apple Silicon, then package AppImage, Debian and Flatpak where offered. Inspect the package metadata, architecture and dependencies.
5. On each operating system manually validate file dialogs, inputs and gamepad switching, fullscreen, audio, save states, FDS paths and application quit/relaunch.
6. Document known accuracy limits and keep the private full per-ROM evidence out of public distribution.
7. Publish an explicit, reviewed tag and release only after the owner approves the final source and assets.

## Packaging entrypoints

| Target | Script / workflow |
| --- | --- |
| Ubuntu native + source ZIP | `./package_release.sh` |
| Windows native ZIP | `./package_release_windows.ps1` |
| macOS app and single-file builds | `./build_macos.sh` |
| AppImage | `./packaging/appimage/build_appimage.sh` |
| Debian package | `./packaging/debian/build_deb.sh` |
| Flatpak | `./packaging/flatpak/build_flatpak.sh` |
| GitHub Actions candidate | `.github/workflows/release-candidate.yml` (draft, not a public release) |
| ROM-free source export | `.github/workflows/source-export.yml` |

Generated output lives under `build/` and `dist/` and must never be committed into the clean source tree. The draft-candidate workflow, if used, requires appropriate repository permissions and must not silently replace an existing tag. Some runner environments cannot upload normal Actions artifacts; a successful build is distinct from a successful artifact publication.

## Limitations to disclose

The [test summary](TESTING.md) includes seven known failing NTSC test entries, other unverified tests, incomplete platform-specific validation, and original-ROM/composite-output caveats. Registered mapper IDs and boot-smoke counts cannot be advertised as fully compatible game counts.

## ROMs and licensing

Do not include downloaded ROM dumps, game-specific screenshots from private surveys, copyrighted system BIOS images, user saves or upstream test-ROM archives without separate distribution permission. Check third-party license and attribution obligations for any shipped runtime library or assets.
