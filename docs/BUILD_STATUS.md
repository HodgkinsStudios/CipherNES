# CipherNES — build and verification status

## Portable runtime update (October 22, 2026)
The current source includes per-user data directories, runtime-created BIOS folder, single-file Windows executable with embedded runtime DLLs, macOS standalone executables containing embedded/re-signed dylibs, and Linux AppImage. Build and smoke validation is tracked in the Actions page for the exact GitHub commit, not inferred from older versions.

Automation covers Ubuntu x86_64, Windows x86_64, macOS x86_64/arm64, Linux AppImage, Debian amd64 and Flatpak. Ubuntu's initial source test build passed locally; first-run folders and synthetic ROM 24-frame save-state round-trip passed. The executable's SDL symbol self-test passed on local Linux.

**Important macOS limitation:** on unattended macOS CI, GUI/HID startup and even dynamic SDL loader diagnostics can hang. CI therefore keeps the optional loader probe time-bounded; native compilation and synthetic-ROM core execution/state tests remain mandatory. A successful workflow is not evidence of a tested desktop GUI, physical controller, sound output, Gatekeeper notarization or universal hardware compatibility.

## v1.0.1 release candidate (October 24, 2026)

The v1.0.1 CP01 CPU/PPU/APU/mapper behavior is intentionally unchanged. A 600-frame rewind cap,
64 MiB allocated-memory bound, and buffer reuse/compression allocation changes are covered by
rewind regression tests; 289/289 valid test images passed the 20-frame smoke round trip with
unchanged CRCs. This is not a fresh complete internal accuracy certification.

New GitHub Actions run results for the v1.0.1 commit must be checked before the release
is marked publishable. See `RELEASE_NOTES_1.0.1.md` for hardware and UI caveats.

## Original v1.0.0 baseline

GitHub Actions six-workflow baseline succeeded on October 22, 2026 at commit `a65eee4551eb24405672bc789e13ca66e215a72d` and produced seven artifacts: Ubuntu, Windows, Intel Mac, Apple Silicon Mac, AppImage, DEB and Flatpak.

The historical AccuracyCoin **146/146** result was obtained for the original trimmed Ubuntu core, **not** independently re-run for every platform's current distribution. Other historical ROM smoke results should not be described as complete compatibility certification.

For the latest build evidence consult [GitHub Actions](https://github.com/HodgkinsStudios/CipherNES/actions), [validation methodology](VALIDATION.md), [portability details](PORTABLE_RUNTIME.md), and [release instructions](GITHUB_RELEASE.md). No copyrighted game ROMs or firmware are redistributed.
