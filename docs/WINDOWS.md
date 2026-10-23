# CipherNES on Windows

CipherNES includes a native Windows frontend built on the same emulator core used by the Ubuntu and macOS versions.

## Toolchain

The Windows project targets **MinGW-w64 GCC** and is suitable for Code::Blocks. C++11 remains the baseline language mode.

### Code::Blocks

Open `CipherNES_Windows.cbp`, select **Release**, and build. This project compiles the core directly into `CipherNES.exe` with `CIPHERNES_STATIC`.

### Command-line build

From PowerShell or Command Prompt:

```bat
build_windows.bat
```

The finished tree is written to:

```text
build\windows\bin\
```

The scripted build produces a single distributable file: `CipherNES.exe`.
The NES emulation core is linked into this file. SDL2, zlib and available MinGW runtime DLLs are embedded as binary PE resources, not shipped as separate files. At launch, these assets are verified against the embedded contents and extracted to the user's `runtime/` folder before SDL2 is loaded. The operating system still supplies its standard Windows DLLs.

## Native file dialogs and timeline shortcuts

Windows uses the Win32 Explorer open-file dialog for ROM/TAS inputs and the system folder chooser for custom AVI paths. The PowerShell build and Code::Blocks project link `comdlg32` and `ole32` along with `shell32`. Rewind is R (hold), fast-forward T (hold), both remappable to controller keys/buttons/triggers. Fast-forward cannot be used during TAS recording. The Windows GitHub Actions workflow passed on October 22, 2026 (commit `a65eee455`). Real-device dialog, audio, and input behavior remains subject to manual verification.

## Per-user storage and FDS BIOS

On first launch CipherNES creates:

```text
%APPDATA%\HodgkinsStudios\CipherNES\
├── Saves\
├── SaveState\
├── Screenshots\
├── Videos\
├── TAS\
├── Cheats\
├── bios\
├── config\
├── logs\
├── roms\
└── runtime\  (self-contained DLL extraction)
```

To use Famicom Disk System images, supply your own legally obtained 8192-byte firmware at `%APPDATA%\HodgkinsStudios\CipherNES\bios\disksys.rom`. Neither firmware nor ROMs are included, and the executable folder remains unwritten. For tests, `CIPHERNES_RUNTIME_DIR` overrides the default.

## CI validation

The Windows GitHub Actions workflow builds the MinGW target, runs the core smoke test, initializes the SDL frontend through `--controller-test`, checks the command-line frontend, audits the release tree, and uploads the build artifact.

## Shared-core compatibility

The Windows, macOS, and Ubuntu targets use the same emulation implementation. Platform-specific host code is kept outside the hardware core.
