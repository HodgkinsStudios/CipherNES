# CipherNES on macOS

**Maintained by HodgkinsStudios**

CipherNES supports native macOS builds on both Intel `x86_64` and Apple Silicon `arm64`.

## Release layout

The macOS build creates `build/macos/CipherNES.app` and packages it as:

- `CipherNES-macOS-x86_64.zip` on Intel Macs.
- `CipherNES-macOS-arm64.zip` on Apple Silicon Macs.

The emulation core and SDL2 runtime libraries are embedded in the Mach-O executable. The release also exposes a single-file `CipherNES-macOS-<arch>-single-file` for terminal launching, alongside the conventional `.app` bundle for Finder launching. SDL2 dylibs are extracted and verified within per-user application data at runtime; Homebrew is only required to build the release, not to run it.

## Building

Requirements:

- macOS 12 or newer.
- Xcode Command Line Tools / `clang++`.
- Homebrew SDL2 for producing the distributable bundle: `brew install sdl2`.

Run:

```bash
chmod +x build_macos.sh
./build_macos.sh
```

The build is native to the machine architecture. GitHub Actions builds and tests Intel and Apple Silicon artifacts independently.

## Runtime files

Writable CipherNES data lives outside the application bundle:

```text
~/Library/Application Support/HodgkinsStudios/CipherNES/
├── SaveState/
├── Saves/
├── Screenshots/
├── TAS/
├── Cheats/
├── bios/
└── ...
```

For Famicom Disk System support, place a legally obtained 8192-byte `disksys.rom` in:

```text
~/Library/Application Support/HodgkinsStudios/CipherNES/bios/disksys.rom
```

CipherNES does not distribute ROMs or the FDS BIOS.

## Native file dialogs and timeline shortcuts

ROM/TAS file selection and the AVI output directory use macOS's native file/folder selection panels via `osascript`; the SDL embedded file browser was removed. The shared Unix frontend includes the same rewind/TAS history and fast-forward controls as Linux. Default hold bindings are R and T, remappable through Controller Input. New UI behavior must be confirmed by the macOS Actions builds and real Apple hardware; the new code has not been runtime-verified on macOS.

## Validation

The CI build verifies:

- Core smoke test.
- Frontend `--help` smoke test.
- macOS app-bundle/release-tree audit.
- Code-signature verification of the ad-hoc signed CI bundle.

GitHub-hosted macOS runners do not provide a representative logged-in GUI/HID session, so hosted CI is used for binary, CLI, bundle, dependency, and signature verification rather than full physical-controller certification. The existing `--controller-test` remains available for testing on a real Mac with the normal SDL controller subsystem enabled.

The app bundle is ad-hoc signed for internal consistency. The separate bare executable may still encounter Gatekeeper quarantine limitations when downloaded from the Internet. A public Gatekeeper-friendly release can later be signed with an Apple Developer ID and notarized without changing the emulator core.
