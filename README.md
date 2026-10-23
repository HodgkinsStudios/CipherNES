<div align="center">

# CipherNES

**A native NES & Famicom emulator for Windows, Linux, and macOS.**

Built with C++ and SDL2 · Cartridge and disk emulation · Save states · Rewind · Gamepad support

[**Download v1.0.0**](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.0) · [Features](docs/FEATURES.md) · [Documentation](docs/README.md) · [Build from source](#building-from-source)

[![Ubuntu](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/ubuntu-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/ubuntu-build.yml)
[![Windows](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/windows-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/windows-build.yml)
[![macOS](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/macos-build.yml/badge.svg?branch=main)](https://github.com/HodgkinsStudios/CipherNES/actions/workflows/macos-build.yml)
[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

</div>

---

## Overview

CipherNES is a cross-platform Nintendo Entertainment System / Famicom emulator. Its native desktop frontends share a C++ emulation core covering the 6502 CPU, 2C02 PPU, 2A03 APU, and multiple cartridge mapper families.

| Emulation | Desktop & gameplay |
| --- | --- |
| iNES and NES 2.0 cartridges | Keyboard and gamepad remapping |
| Famicom Disk System with a **user-supplied BIOS** | Save RAM, save states, screenshots, Game Genie |
| NTSC-oriented timing and selectable hardware profiles | Rewind, 150% fast-forward, TAS recording/playback |
| `.nes`, `.fds`, `.qd`, and supported `.zip` inputs | File pickers, drag and drop, windowed/fullscreen modes |

Compatibility varies by game and mapper. See the [feature guide](docs/FEATURES.md), [mapper reference](docs/NTSC_MAPPERS.md), and [compatibility notes](docs/NTSC_COMPATIBILITY.md).

> **Bring your own games.** CipherNES does not include commercial ROMs, proprietary FDS firmware, or third-party test ROMs.

## Downloads

All files are available in the **[CipherNES 1.0 GitHub Release](https://github.com/HodgkinsStudios/CipherNES/releases/tag/1.0)**.

| Platform | Recommended | Other options |
| --- | --- | --- |
| **Windows (x86-64)** | [Portable EXE](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-Windows-x86_64.exe) | — |
| **Linux (x86-64)** | [AppImage](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-Linux-x86_64.AppImage) | [Native ZIP](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-Linux-x86_64.zip) |
| **Debian / Ubuntu (amd64)** | [DEB package](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-Debian-amd64.deb) | — |
| **Linux with Flatpak** | [Flatpak bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-Linux-x86_64.flatpak) | — |
| **macOS (Apple Silicon)** | [App bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-macOS-arm64.app.zip) | [Portable binary](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-macOS-arm64-portable) |
| **macOS (Intel)** | [App bundle](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-macOS-x86_64.app.zip) | [Portable binary](https://github.com/HodgkinsStudios/CipherNES/releases/download/1.0/CipherNES-1.0.0-macOS-x86_64-portable) |

**What “portable” means:** The Windows EXE, macOS portable binaries, and Linux AppImage are single-file *downloads*. Windows and macOS extract embedded runtime libraries into the user's app-data `runtime/` directory when needed. The macOS portable executables have **no filename extension**; the AppImage keeps its `.AppImage` extension. macOS distributions are not Apple-notarized and may require Gatekeeper approval.

### Quick start

**Windows:** Download the `.exe` and double-click it.

**Linux AppImage:** In a terminal opened in the download directory:

```bash
chmod +x CipherNES-1.0.0-Linux-x86_64.AppImage
./CipherNES-1.0.0-Linux-x86_64.AppImage
```

If AppImage FUSE support is unavailable:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./CipherNES-1.0.0-Linux-x86_64.AppImage
```

**Debian / Ubuntu package:**

```bash
sudo apt install ./CipherNES-1.0.0-Debian-amd64.deb
```

**Flatpak bundle:**

```bash
flatpak install --user ./CipherNES-1.0.0-Linux-x86_64.flatpak
flatpak run io.github.hodgkinsstudios.CipherNES
```

**macOS:** Extract the `.app.zip` for your processor and open `CipherNES.app`. Alternatively, mark the extensionless portable binary executable with `chmod +x` and run it from Terminal. See the [macOS guide](docs/MACOS.md) for details.

## Controls

| Action | Default |
| --- | --- |
| Rewind | Hold **R** |
| Fast-forward | Hold **T**, up to **150%** |
| Switch display modes | **Esc** — windowed → aspect-correct fullscreen → stretched fullscreen |
| Rebind keyboard/gamepad | **Configure → Controller Input** |
| Save states, screenshots, and TAS recording | Application menus |

Fast-forward is disabled while actively recording TAS inputs; video capture also restricts some timeline actions. See [Features & Controls](docs/FEATURES.md) for the complete behavior.

## First launch and user data

CipherNES creates its writable directories on first launch, **not beside the executable**.

| Platform | Data directory |
| --- | --- |
| **Linux / AppImage** | `~/.local/share/HodgkinsStudios/CipherNES/` |
| **Windows** | `%APPDATA%\HodgkinsStudios\CipherNES\` |
| **macOS** | `~/Library/Application Support/HodgkinsStudios/CipherNES/` |
| **Flatpak** | `~/.var/app/io.github.hodgkinsstudios.CipherNES/data/HodgkinsStudios/CipherNES/` |

On first launch it creates `Saves/`, `SaveState/`, `Screenshots/`, `Videos/`, `TAS/`, `Cheats/`, `roms/`, `bios/`, `config/`, and `logs/`. Existing saves from executable-adjacent folders may need to be moved manually.

**Famicom Disk System:** Place a legally obtained **8,192-byte** BIOS at `<data directory>/bios/disksys.rom`. The `bios/` folder starts empty; firmware is never bundled. `CIPHERNES_RUNTIME_DIR` can override the data root for isolated tests.

More details: [Portable Runtime & Storage](docs/PORTABLE_RUNTIME.md).

## Building from source

Clone the repository and run the appropriate build script:

```bash
git clone https://github.com/HodgkinsStudios/CipherNES.git
cd CipherNES
```

| System | Requirements | Command |
| --- | --- | --- |
| **Ubuntu / Linux** | GCC/G++, SDL2 runtime, zlib, build tools | `./build_ubuntu.sh` |
| **Windows** | MinGW-w64 with SDL2/zlib build dependencies | `build_windows.bat` or `build_windows.ps1` |
| **macOS** | Xcode Command Line Tools, Homebrew SDL2 | `./build_macos.sh` |

**Ubuntu example:**

```bash
sudo apt update
sudo apt install -y build-essential libsdl2-2.0-0 zlib1g
./build_ubuntu.sh
./build/ubuntu/bin/CipherNES
```

Build outputs include `build/ubuntu/bin/CipherNES`, `build/windows/bin/CipherNES.exe`, and `build/macos/CipherNES.app`. The project also supports AppImage, Debian, and Flatpak packaging through [GitHub Actions](https://github.com/HodgkinsStudios/CipherNES/actions).

Read [Windows build instructions](docs/WINDOWS.md), [macOS build instructions](docs/MACOS.md), and the [release guide](docs/GITHUB_RELEASE.md) for platform-specific details.

## Accuracy and validation

An earlier verified Ubuntu 1.0 core run passed **146 / 146 AccuracyCoin tests**:

![AccuracyCoin test result — 146 of 146 passed](docs/accuracycoin_146_146.png)

A separate external NTSC ROM archive completed **289 / 289** valid iNES image load/execution/save-state round-trip smoke checks (600 frames per ROM). **Those smoke checks do not establish individual internal test-ROM PASS results.**

CI builds and runs selected smoke/regression checks for Windows, Linux, macOS, AppImage, Debian, and Flatpak. Real-device testing of presentation, audio, controls, and macOS security prompts is still important; Flatpak native file-dialog behavior is not fully certified.

Details: [Validation Methodology](docs/VALIDATION.md) · [Historical Accuracy Results](docs/NTSC_ACCURACY.md) · [Build Status](docs/BUILD_STATUS.md).

## Documentation

| Guide | Topics |
| --- | --- |
| [Documentation Index](docs/README.md) | All guides and technical references |
| [Features & Controls](docs/FEATURES.md) | Input, saves, rewind, TAS, video/display |
| [Mapper Reference](docs/NTSC_MAPPERS.md) | Mapper support and board families |
| [System Architecture](docs/SYSTEM_ARCHITECTURE.md) | Core and frontend design |
| [Core API](docs/CORE_LIBRARY.md) | Native emulation interface |
| [Release Notes](RELEASE_NOTES_1.0.0.md) | What's included in v1.0.0 |

## License and acknowledgements

Created by **Jacob Hodgkins / HodgkinsStudios**. CipherNES is available under the [MIT License](LICENSE).

See [Third-Party Notices](THIRD_PARTY_NOTICES.md) for dependency credits. External NES test ROMs retain their original authorship; the project acknowledges [christopherpow/nes-test-roms](https://github.com/christopherpow/nes-test-roms). No copyrighted game files, proprietary FDS BIOS, personal saves, or third-party test-ROM suites are distributed with CipherNES.
