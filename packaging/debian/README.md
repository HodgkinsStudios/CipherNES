# Debian / Ubuntu package

This directory contains the native Debian/Ubuntu packaging for CipherNES.

## Build

From the repository root:

```bash
./packaging/debian/build_deb.sh
```

The script first runs the normal Ubuntu build and then creates:

```text
dist/CipherNES-<version>-<architecture>.deb
```

For an already-built Ubuntu tree, skip recompilation with:

```bash
CIPHERNES_SKIP_BUILD=1 ./packaging/debian/build_deb.sh
```

`CIPHERNES_BIN_DIR` can point to another directory containing `CipherNES` and
the core linked into `CipherNES`.

## Runtime data

The package installs the application under `/usr/lib/ciphernes` and the launcher
as `/usr/bin/ciphernes`. Writable data is kept under:

```text
${XDG_DATA_HOME:-~/.local/share}/ciphernes/
```

This includes saves, save states, screenshots, recordings, cheats, TAS data,
and the optional FDS BIOS at `bios/disksys.rom`.

The FDS BIOS is not distributed with CipherNES.

## Native file dialogs

The installed desktop frontend uses the operating system file chooser via
`zenity` (GTK) or `kdialog` (KDE) rather than an embedded browser. The package
**recommends** these tools; install one if it is missing on your desktop.
