# AppImage V2

CipherNES AppImage V2 is the current portable Linux package for CipherNES.

The package uses the standard AppDir layout and `appimagetool` to create a
type-2 AppImage.

## Build

On x86-64 Ubuntu/Debian, install the build prerequisites:

```bash
sudo apt update
sudo apt install build-essential libsdl2-2.0-0 zlib1g curl desktop-file-utils appstream
```

Then run:

```bash
./packaging/appimage/build_appimage.sh
```

Output:

```text
dist/CipherNES-1.0.1-x86_64.AppImage
```

The generated AppImage itself is a release artifact and must not be committed
to Git.

## Run

```bash
chmod +x CipherNES-1.0.1-x86_64.AppImage
./CipherNES-1.0.1-x86_64.AppImage
```

A ROM can be supplied directly:

```bash
./CipherNES-1.0.1-x86_64.AppImage /path/to/game.nes
```

If FUSE is unavailable, current type-2 AppImages also support:

```bash
./CipherNES-1.0.1-x86_64.AppImage --appimage-extract-and-run
```

## Bundled runtime components

The AppImage includes:

- CipherNES
- the core linked into `CipherNES`
- SDL2
- zlib
- GCC C++ runtime libraries
- desktop/AppStream metadata
- CipherNES application icon
- license/documentation files

Low-level libraries such as glibc and hardware/graphics-driver libraries stay host-provided.

## Runtime data

Writable application data is kept outside the read-only AppImage at:

```text
${XDG_DATA_HOME:-~/.local/share}/ciphernes/
```

This includes saves, save states, screenshots, recordings, cheats, TAS data,
and the optional FDS BIOS:

```text
~/.local/share/ciphernes/bios/disksys.rom
```

The FDS BIOS is not distributed with CipherNES.

## Native file dialogs

ROM/TAS selection and AVI output folder selection use the host operating
system file chooser. Install `zenity` or `kdialog` on the Linux host, since
the portable AppImage does not bundle the desktop file chooser executable.
