<div align="center">

# CipherNES

**A native NES & Famicom emulator for Windows, Linux and macOS**

**Created by Jacob Hodgkins · HodgkinsStudios**

Modern desktop controls · Shared C++ emulation core · Save states · Rewind · TAS tools

[**Download CipherNES v1.0.1**](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.01) · [Quick start](#quick-start) · [Features](#features) · [Build](#building-from-source) · [Documentation](docs/README.md)

[![Ubuntu CI](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/ubuntu-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/ubuntu-build.yml)
[![Windows CI](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/windows-build.yml)
[![macOS CI](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/macos-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/macos-build.yml)
[![AppImage CI](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/appimage-package.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/appimage-package.yml)
[![Flatpak CI](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/flatpak-package.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/flatpak-package.yml)
[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

</div>

---

## What is CipherNES?

CipherNES is a cross-platform Nintendo Entertainment System / Famicom emulator. Windows, Ubuntu/Linux and macOS frontends use the same native C++ emulation core, with CPU, PPU, APU, cartridge-mapper and disk-system implementations kept separate from platform UI code.

The emulator targets **NTSC NES/Famicom timing**, provides native file selection and configurable input, and supports conveniences such as save states, reverse-time gameplay, Game Genie codes and tool-assisted input recording.

**Current version: 1.0.1.** The published GitHub Release uses tag [`1.01`](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.01); the version inside the source and downloadable filenames is `1.0.1`. [Previous v1.0.0 release](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.0).

## Download v1.0.1

Choose the package for your system. These are links to **actual assets** in the [published CipherNES 1.01 release](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.01).

| Platform | Recommended download | Alternative |
| :--- | :--- | :--- |
| Windows 10/11, x86-64 | [Portable Windows EXE](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Windows-x86_64.exe) | — |
| Ubuntu/Linux, x86-64 | [AppImage](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Linux-x86_64.AppImage) | [Ubuntu native ZIP](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Ubuntu-x86_64.zip) |
| Debian/Ubuntu, amd64 | [DEB package](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Debian-amd64.deb) | [AppImage](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Linux-x86_64.AppImage) |
| Linux with Flatpak | [Flatpak bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Linux-x86_64.flatpak) | — |
| macOS, Apple Silicon (arm64) | [Application bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-macOS-arm64.app.zip) | [Terminal executable](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-macOS-arm64-portable) |
| macOS, Intel (x86-64) | [Application bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-macOS-x86_64.app.zip) | [Terminal executable](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-macOS-x86_64-portable) |
| Source | [v1.0.1 source ZIP](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.01/CipherNES-1.0.1-Source.zip) | [Browse `main`](https://github.com/HodgkinsStudios/CipherNES/tree/main) |

The Windows and macOS standalone executables unpack verified runtime libraries into per-user application storage when necessary. The Linux native ZIP requires installed system runtime libraries. macOS builds are **not Apple-notarized**, so Gatekeeper approval may be required. Flatpak file-picker integration still needs additional desktop testing.

## What's new in 1.0.1?

This is a **performance and stability update**, not a rewrite of the emulated hardware.

- **Short, bounded rewind:** stores at most **600 NTSC frames** (about **9.97 seconds**), automatically discarding older history.
- **Controlled rewind memory:** independent **64 MiB** history allocation limit, accounting for reserved buffer capacity.
- **Lower per-frame overhead:** less compressed-video buffer over-allocation and reusable save-state serialization scratch memory.
- **Timing preserved:** NTSC target remains approximately **60.0988 FPS**; CPU, PPU, APU, mapper behavior and core save-state format were intentionally kept unchanged.
- **Stronger regressions:** added checks for rewind eviction, exact state and framebuffer restoration, memory pressure and TAS branching.
- **Release synchronization:** source, platform package metadata and CI workflows updated for v1.0.1.

All seven GitHub Actions workflows passed for [release source commit `43dbd3f`](https://github.com/HodgkinsStudios/CipherNES/commit/43dbd3fa5f638a9498cfbaf5e2039dd9fc381a49), including both macOS architectures. **Successful CI and emulation benchmarks do not guarantee perfectly smooth 100% speed on every physical machine.**

See the [full 1.0.1 release notes](RELEASE_NOTES_1.0.1.md) and [changelog](CHANGELOG.md).

## Features

| Emulation & game support | Desktop experience & tools |
| :--- | :--- |
| NES / Famicom cartridge loading with iNES and NES 2.0 headers | Keyboard and gamepad binding configuration |
| `.nes`, `.fds`, `.qd`, plus supported `.zip` files | Native open-file dialogs and drag-and-drop loading |
| Famicom Disk System (with user-provided BIOS) | Battery saves, save slots, screenshots and Game Genie |
| NTSC timing and selectable hardware profiles | ~10-second rewind with reverse captured audio |
| Implemented mapper and board families | Up to 150% fast-forward with audio resampling |
| Core separated from platform frontends | TAS input recording/playback and optional AVI capture |
| Windows, Linux, Intel/Apple Silicon macOS | Windowed, aspect-correct fullscreen and stretched fullscreen |

The [mapper reference](docs/NTSC_MAPPERS.md) documents **151 mapper/disk-system IDs**, including alternate board implementations. That count is *not* a promise that every game using those IDs is fully compatible. See the [compatibility notes](docs/NTSC_COMPATIBILITY.md).

## Quick start

1. Download the appropriate build above.
2. Open CipherNES (installation notes below).
3. Choose the ROM-loading option in the **File** menu, or drag and drop a supported ROM where available.
4. Adjust keyboard or controller bindings through **Configure → Controller Input**.

**Windows:** Download `CipherNES-1.0.1-Windows-x86_64.exe` and run it. Its required packaged runtime components are embedded and extracted on demand.

**Linux AppImage:**

```bash
chmod +x CipherNES-1.0.1-Linux-x86_64.AppImage
./CipherNES-1.0.1-Linux-x86_64.AppImage
```

If FUSE support is unavailable, try `APPIMAGE_EXTRACT_AND_RUN=1 ./CipherNES-1.0.1-Linux-x86_64.AppImage`.

**Debian / Ubuntu DEB:**

```bash
sudo apt install ./CipherNES-1.0.1-Debian-amd64.deb
```

**Flatpak bundle:**

```bash
flatpak install --user ./CipherNES-1.0.1-Linux-x86_64.flatpak
flatpak run io.github.hodgkinsstudios.CipherNES
```

**macOS:** Unzip the appropriate `.app.zip` and open `CipherNES.app`. The separately offered portable Mac downloads are extensionless executables intended for Terminal. If macOS blocks an unsigned or unnotarized app, review the warning and follow the [macOS guide](docs/MACOS.md).

**Ubuntu native ZIP:** Extract it, ensure SDL2 and zlib runtime libraries are installed (`libsdl2-2.0-0` and `zlib1g` on supported Ubuntu versions), and launch the provided executable. Installing `zenity` or `kdialog` enables the Linux native file picker.

### Default controls

| Action | Default |
| :--- | :--- |
| Rewind (while held) | **R** |
| Fast-forward (while held) | **T** — up to 150% |
| Switch display modes | **Esc** — windowed → aspect-correct fullscreen → stretched fullscreen |
| Change keyboard/gamepad bindings | **Configure → Controller Input** |
| Save/load states, capture screenshots, record/play TAS | Application menus |

Rewind and fast-forward can be rebound; **Esc's display-mode action is reserved**. Fast-forward is disabled during active TAS recording. Forward-only AVI capture restricts timeline operations. See the [controls and timeline guide](docs/FEATURES.md).

## Saves, firmware and user files

CipherNES stores user data **outside the installed executable folder**:

| Platform | Default application data directory |
| :--- | :--- |
| Ubuntu / native Linux / AppImage | `~/.local/share/HodgkinsStudios/CipherNES/` |
| Windows | `%APPDATA%\HodgkinsStudios\CipherNES\` |
| macOS | `~/Library/Application Support/HodgkinsStudios/CipherNES/` |
| Flatpak | `~/.var/app/io.github.hodgkinsstudios.CipherNES/data/HodgkinsStudios/CipherNES/` |

On launch, the application creates folders including `Saves/`, `SaveState/`, `Screenshots/`, `Videos/`, `TAS/`, `Cheats/`, `bios/`, `roms/`, `config/` and `logs/`. Existing files are retained. Earlier installs using executable-adjacent save folders may need a manual move.

For **Famicom Disk System** games, provide your own legitimately obtained **8,192-byte** `disksys.rom` at:

```text
<application data directory>/bios/disksys.rom
```

No FDS BIOS ships with CipherNES. For isolated development/testing, `CIPHERNES_RUNTIME_DIR` overrides the storage root. More information: [portable runtime and first-launch storage](docs/PORTABLE_RUNTIME.md).

## Building from source

CipherNES uses **C++11**-compatible source and native build scripts, with Code::Blocks projects for Ubuntu and Windows. Platform frontends are linked to a shared native emulation implementation.

**Ubuntu/Linux (GCC):**

```bash
sudo apt update
sudo apt install -y build-essential libsdl2-2.0-0 zlib1g zenity
git clone https://github.com/HodgkinsStudios/CipherNES.git
cd CipherNES
./build_ubuntu.sh
./build/ubuntu/bin/CipherNES
```

The Ubuntu build includes release-mode optimizations. `CIPHERNES_NATIVE=1 ./build_ubuntu.sh` additionally enables host-specific CPU optimization; those binaries should **not** be distributed as generic builds.

**Windows:** Use MinGW-w64 with SDL2/zlib build dependencies and run `build_windows.bat` or `build_windows.ps1`, or open `CipherNES_Windows.cbp` in Code::Blocks. Output: `build/windows/bin/CipherNES.exe`. See [Windows build notes](docs/WINDOWS.md).

**macOS:** Install Xcode Command Line Tools and Homebrew SDL2 (`brew install sdl2`); run `./build_macos.sh`. Output: `build/macos/CipherNES.app`. See [macOS build notes](docs/MACOS.md).

The [GitHub Actions workflows](https://github.com/HodgkinsStudios/CipherNES/actions) produce automated builds and package archives. For release creation, see [release instructions](docs/GITHUB_RELEASE.md).

## Accuracy and validation

The project keeps emulation accuracy evidence separate from basic load and build success.

**Historical AccuracyCoin result:** An earlier v1.0.0 Ubuntu core run completed **146 / 146 tests (100%)**:

![Earlier Ubuntu AccuracyCoin result: 146 / 146 passed](docs/accuracycoin_146_146.png)

**v1.0.1 regression evidence:** 289 valid iNES files in an externally supplied test archive each loaded, ran **20 frames**, and completed a save-state round trip with **no framebuffer CRC differences against the matched v1.0.0 baseline**. This is a load/execution/regression smoke check; it does **not** evaluate each test ROM's own PASS/FAIL condition. The supplied AccuracyCoin ROM separately passed a 600-frame execution and state-round-trip check, but the complete 146-test internal verdict was **not** freshly re-certified for v1.0.1.

Windows, Ubuntu, macOS (Intel and Apple Silicon), Debian, AppImage, Flatpak and source-export automation completed successfully on the v1.0.1 release commit. Native GUI presentation, audio latency, physical-controller behavior and device-specific fullscreen pacing still warrant hands-on testing.

- [Validation methodology and reproduction](docs/VALIDATION.md)
- [Historical NTSC accuracy record](docs/NTSC_ACCURACY.md)
- [Build verification status](docs/BUILD_STATUS.md)
- [Mapper compatibility](docs/NTSC_MAPPERS.md)

Third-party test ROMs are not bundled. A source of external tests used for validation is [christopherpow/nes-test-roms](https://github.com/christopherpow/nes-test-roms); individual authors and licenses remain with the original projects.

## Documentation

| Reference | Contents |
| :--- | :--- |
| [Documentation index](docs/README.md) | All user and developer guides |
| [Features and controls](docs/FEATURES.md) | Rewind, fast-forward, gamepads, TAS and display modes |
| [System architecture](docs/SYSTEM_ARCHITECTURE.md) | Shared core and platform-specific frontends |
| [Core library / API](docs/CORE_LIBRARY.md) | C API, ABI and state compatibility |
| [Mapper implementation list](docs/NTSC_MAPPERS.md) | Supported mapper IDs and board families |
| [Windows](docs/WINDOWS.md) · [macOS](docs/MACOS.md) | Platform-specific builds and limitations |
| [Release packaging](docs/GITHUB_RELEASE.md) | GitHub Actions assets and release process |
| [Changelog](CHANGELOG.md) | Version-by-version changes |

## License and acknowledgements

**CipherNES — Created by Jacob Hodgkins / HodgkinsStudios.**

Distributed under the [MIT License](LICENSE). Third-party libraries and contributions are credited in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

CipherNES is an independent emulator project. It does **not** distribute commercial game ROMs, copyrighted third-party test suites, proprietary Nintendo firmware or personal save files. Nintendo, NES and Famicom are trademarks of their respective owners.
