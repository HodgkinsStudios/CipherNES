# Flatpak package

This directory contains the Flatpak packaging for CipherNES.

Application ID:

```text
io.github.hodgkinsstudios.CipherNES
```

Runtime:

```text
org.freedesktop.Platform//25.08
```

## Build requirements

Install Flatpak, flatpak-builder, and the Freedesktop 25.08 SDK/runtime. With
Flathub configured:

```bash
flatpak install flathub org.freedesktop.Platform//25.08 org.freedesktop.Sdk//25.08
```

Then build from the repository root:

```bash
./packaging/flatpak/build_flatpak.sh
```

Output:

```text
dist/CipherNES-1.0.1-x86_64.flatpak
```

## Install

```bash
flatpak install --user ./dist/CipherNES-1.0.1-x86_64.flatpak
flatpak run io.github.hodgkinsstudios.CipherNES
```

## Runtime data

Flatpak redirects XDG application data into the app sandbox. CipherNES keeps its
writable data under:

```text
~/.var/app/io.github.hodgkinsstudios.CipherNES/data/ciphernes/
```

That directory contains saves, save states, screenshots, recordings, cheats,
TAS data, and the optional FDS BIOS:

```text
~/.var/app/io.github.hodgkinsstudios.CipherNES/data/ciphernes/bios/disksys.rom
```

## Sandbox permissions and native file dialogs

CipherNES needs graphics, audio, and controller access. Its native Linux file
chooser calls the desktop's `zenity` or `kdialog` program, instead of rendering
an embedded file browser. However, **a host-installed chooser binary is not
exposed inside a Flatpak sandbox**, and this Flatpak manifest does not yet
bundle a chooser or implement the XDG FileChooser portal. As a result,
interactive ROM/TAS selection and selecting custom AVI folders are **not yet
guaranteed inside the Flatpak build**. Launching a ROM by command-line path
can still be used when the filesystem permissions allow it; users requiring
native interactive selection should use the .deb, AppImage, or Ubuntu build.
Do not label the Flatpak native picker as verified in release notes or CI.

The manifest grants read-only home and removable-media access for existing
ROM files. A future portal-backed chooser should replace the file-system
permissions with per-file access from the host portal.

## Flathub

The manifest uses the GitHub-derived application ID `io.github.hodgkinsstudios.CipherNES`. A future Flathub submission can reuse the same manifest and metadata with the source commit updated for that release.
