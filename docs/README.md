# CipherNES documentation

Welcome to the documentation for **CipherNES 1.0.0**, a cross-platform NES/Famicom emulator. Start with the user-facing guides, then consult the architecture and validation references for implementation details and recorded evidence.

## Using and building CipherNES

| Guide | Contents |
| --- | --- |
| [Features and controls](FEATURES.md) | Rewind, fast-forward, input configuration, saves, TAS, and display modes |
| [Windows](WINDOWS.md) | MinGW-w64 and Code::Blocks build instructions, runtime DLLs, and BIOS location |
| [macOS](MACOS.md) | Intel and Apple Silicon builds, application bundle, and data paths |
| [GitHub releases](GITHUB_RELEASE.md) | CI verification, release asset checklist, and publishing workflow |
| [Build verification](BUILD_STATUS.md) | CI build matrix, downloaded artifacts, and test limitations |

For Ubuntu, run `./build_ubuntu.sh` from the repository root after installing the compiler, SDL2, and zlib dependencies. The resulting executable is in `build/ubuntu/bin/`.

## Technical reference

| Reference | Contents |
| --- | --- |
| [System architecture](SYSTEM_ARCHITECTURE.md) | Shared emulation core, host frontend, and state and timing design |
| [Core C API](CORE_LIBRARY.md) | Public ABI v10, compatibility, and removed interfaces |
| [Core audit](CORE_AUDIT.md) | API-surface reduction and impact on callers |
| [NTSC compatibility](NTSC_COMPATIBILITY.md) | Scope of supported timing and compatibility statements |
| [Mapper reference](NTSC_MAPPERS.md) | Mapper IDs, families, and special hardware profiles |
| [Validation](VALIDATION.md) | Current test evidence, reproduction, and caveats |
| [Historical NTSC results](NTSC_ACCURACY.md) | Recorded formal test-ROM outcomes and limitations |

## Release resources

- [Release notes](../RELEASE_NOTES_1.0.0.md) and [changelog](../CHANGELOG.md).

**Distribution policy:** The project does not bundle ROMs, FDS firmware, personal saves, or copyrighted third-party test images. External test-suite attribution: [christopherpow/nes-test-roms](https://github.com/christopherpow/nes-test-roms).

- [Portable runtime and first-launch storage](PORTABLE_RUNTIME.md) — OS-specific paths, BIOS setup, single-file packaging, and migration.
