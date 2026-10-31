# Portable runtime and first-launch data policy

CipherNES separates read-only executable artifacts from user-owned runtime data. This change does not introduce or distribute game ROMs or Famicom Disk System firmware.

## Canonical data roots

| OS | Application data directory |
| --- | --- |
| Linux (including native and AppImage) | `~/.local/share/HodgkinsStudios/CipherNES/` |
| Windows | `%APPDATA%\HodgkinsStudios\CipherNES\` |
| macOS | `~/Library/Application Support/HodgkinsStudios/CipherNES/` |

CipherNES intentionally uses `~/.local/share` on Linux regardless of `XDG_DATA_HOME` to honor its canonical location. Flatpak overrides the runtime root to its sandbox-owned `XDG_DATA_HOME/HodgkinsStudios/CipherNES`, typically `~/.var/app/io.github.hodgkinsstudios.CipherNES/data/HodgkinsStudios/CipherNES`, because Flatpak grants the application a private writable data folder while mounting the normal home directory read-only. A test may override any root with the `CIPHERNES_RUNTIME_DIR` environment variable; use an absolute path.

On every launch, ensure the root and the following folders exist (without clearing existing files): `Saves`, `SaveState`, `Screenshots`, `Videos`, `TAS`, `Cheats`, `bios`, `config`, `logs`, `roms`. The configuration file `CipherNES.cfg` is stored directly in the data root. Battery saves are read/written under `Saves`, state slots under `SaveState`, captured images under `Screenshots`, recordings under `Videos`, TAS inputs under `TAS`, and cheat sets under `Cheats`.

FDS firmware: only look for the user-provided **8192-byte** `bios/disksys.rom` under this data root. Do not search or bundle a BIOS alongside executable files. First launch creates an **empty** `bios` directory. Users upgrading from older versions can manually copy files out of the former executable-adjacent directories.

## Packaging and portability

- Windows portable **single `CipherNES.exe`** embeds the core and SDL2/zlib binary resources, checking and extracting the latter to `runtime/` on launch. They are verified against embedded copies, avoiding stale/mismatched external copies.
- macOS **single Mach-O executable** includes its core and embedded SDL dylibs and extracts the latter to `runtime/` on launch. A usual `.app` bundle is also built for Finder/Gatekeeper integration. Public releases may require Developer ID signing/notarization.
- Linux's **AppImage** is a single downloadable executable container with SDL2 and zlib. The plain native ELF has its core statically linked but relies on installed SDL2 and system libraries; `.deb` and Flatpak manage runtimes separately.

Embedded DLLs/dylibs are not a license to ship ROMs or firmware. The operating system's base libraries cannot and should not be completely bundled as part of an application.

## Tests

`--help` initializes directories and exercises resolution of the runtime root. Linux CI verifies canonical paths and the `CIPHERNES_RUNTIME_DIR` override. GitHub Actions verifies the bare Windows .exe launch without adjacent DLLs and the bare macOS Mach-O executable; CI smoke checks are not a substitute for physical-device tests.
