# Building and running CipherNES

All three desktop targets compile the same source from `CipherNES_Core/`. The core interface is declared in `CipherNES_Core/ciphernes_core.h`; build scripts link the native implementation into each executable. Compiler switches target a C++11-compatible toolchain (newer toolchains also work).

## Linux / Ubuntu

Install dependencies and build from the source root:

```bash
sudo apt update
sudo apt install -y build-essential libsdl2-2.0-0 zlib1g zenity
CIPHERNES_LTO=0 CIPHERNES_JOBS=4 ./build_ubuntu.sh
./build/ubuntu/bin/CipherNES
```

`zenity` provides the graphical file chooser; `kdialog` is an alternative. `./run_ciphernes.sh` launches an existing build. The Linux executable uses host SDL2/zlib runtime libraries; bundling is handled by platform-specific packaging tools.

**Optional packages:** AppImage, Debian, and Flatpak scripts live under `packaging/appimage/`, `packaging/debian/` and `packaging/flatpak/`; each has its own README. `./package_release.sh` makes a native Ubuntu ZIP and ROM-free source ZIP. Packaging output belongs under generated `dist/` and must not enter a source checkpoint.

## Windows

Build using MinGW-w64 and SDL2/zlib developer components:

```bat
build_windows.bat
```

PowerShell: `./build_windows.ps1`. Alternatively open `CipherNES_Windows.cbp` in Code::Blocks and build the release target. The output is under `build\windows\bin\`.

The Windows frontend includes Win32 native file/folder dialogs. The release-oriented executable embeds the needed SDL2/zlib binary runtime resources and verifies/extracts them to the user's private `runtime/` folder; Windows system libraries remain external. `package_release_windows.ps1` handles Windows packaging.

The project includes CI workflows for a Windows smoke build, but there is **no claim of an independently completed Windows build in this cleanup environment**. Native launch, dialogs, audio, controller and single-file packaging must be retested on Windows before shipping.

## macOS

Requires macOS 12+, Xcode Command Line Tools, and Homebrew SDL2 for building:

```bash
brew install sdl2
./build_macos.sh
```

The script targets the host architecture and produces `build/macos/CipherNES.app` and associated artifacts. Intel x86_64 and Apple Silicon arm64 targets should be validated independently. A normal `.app` and an optional single-file Mach-O target are supported by the existing packaging scripts; embedded SDL runtime extraction and system security requirements (including signing/notarization) must be verified on actual machines. The macOS Intel and Apple Silicon build jobs passed on GitHub Actions for the updated branding commit; native device interaction, sound, gamepad and notarization/signing remain separate release checks.

## Per-user data paths

| Platform | Default directory |
| --- | --- |
| Linux | `~/.local/share/HodgkinsStudios/CipherNES/` |
| Windows | `%APPDATA%\HodgkinsStudios\CipherNES\` |
| macOS | `~/Library/Application Support/HodgkinsStudios/CipherNES/` |

Linux defaults to the fixed `~/.local/share` directory; Flatpak uses its private sandbox data root (typically under `~/.var/app/io.github.hodgkinsstudios.CipherNES/data/`). `CIPHERNES_RUNTIME_DIR` can redirect the runtime root for **isolated tests**. The application creates save, state, screenshot, recording, TAS, cheat, config, BIOS and log subfolders.

For FDS, place an owner-supplied, legally obtained **8192-byte** `disksys.rom` in the per-user `bios/` subdirectory. Do not bundle firmware with executable or source artifacts.

## Build/CI boundaries

- Ubuntu compilation and ROM-independent regressions have been executed on the current cleanup tree; see [testing](TESTING.md).
- Windows, macOS, AppImage, Debian, and Flatpak packages need final-commit CI and device tests before public release.
- No compiler output (`build/`, `dist/`, `obj/`, executables, libraries) belongs in the committed source archive.
