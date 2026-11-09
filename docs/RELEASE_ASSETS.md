# CipherNES 1.0.0 release assets

The default-branch build workflows compile and smoke-test the shared `CipherNES_Core` on Linux, Windows and both macOS architectures. They upload outputs directly to a **draft** GitHub Release at `v1.0.0` because this repository has exhausted its GitHub Actions artifact quota.

## Draft asset inventory

| Platform | Expected download |
| --- | --- |
| Ubuntu 64-bit | `CipherNES-1.0.0-Ubuntu-x86_64.zip` |
| Linux standalone executable | `CipherNES` (requires system SDL2 and zlib) |
| Windows 64-bit | `CipherNES-1.0.0-Windows-x86_64.zip` |
| Windows standalone executable | `CipherNES.exe` (embedded SDL2/zlib runtime) |
| macOS Intel | `CipherNES-macOS-x86_64.zip`, `CipherNES-macOS-x86_64-single-file` |
| macOS Apple Silicon | `CipherNES-macOS-arm64.zip`, `CipherNES-macOS-arm64-single-file` |
| Linux AppImage | `CipherNES-1.0.0-x86_64.AppImage` and SHA-256 |
| Debian/Ubuntu .deb | `CipherNES-1.0.0-amd64.deb` and SHA-256 |
| Linux Flatpak | `CipherNES-1.0.0-x86_64.flatpak` and SHA-256 |
| Source (no third-party ROMs) | `CipherNES-1.0.0-GitHub-Source.zip` |

## Publishing

1. Open <https://github.com/HodgkinsStudios/CipherNES/releases> as the repository owner and open the **draft** `v1.0.0` release.
2. Confirm all expected platform assets are attached and the builds on the exact tagged commit have finished successfully.
3. Download and test Windows and macOS desktop interactions on real machines. Hosted smoke tests are not a substitute for real graphics/audio/input verification.
4. Review the notes and click **Publish release** when satisfied.

Uploading assets to an existing tag from a different commit is explicitly blocked. Update `VERSION` for subsequent releases rather than overwriting old binaries.

Neither emulator packages nor the source export contain commercial game ROMs or FDS BIOS firmware.
