# User guide

CipherNES provides desktop frontends for Linux, Windows, and macOS. The exact menu appearance can differ by host platform; the emulation core is shared.

## Core controls

| Action | Default |
| --- | --- |
| Open a ROM or ZIP | File → Open, command-line path, or supported drag-and-drop |
| Controller mapping | Configure → Controller Input |
| Display mode cycle | `Esc`: windowed → aspect-preserving fullscreen → stretched fullscreen |
| Rewind | Hold `R` |
| Fast-forward | Hold `T` |
| Save/load state | Save slot commands in desktop frontend |
| TAS recording | TAS menu (when enabled) |

Controller bindings support keyboard and gamepad buttons, axes, hats and triggers. Hot-plugging and switches between input devices are frontend responsibilities. Check the target-platform UI before relying on a specific shortcut or dialog.

## Emulator features

**Rewind:** Stores bounded frame history, machine state, image deltas, captured PCM and recorded input cursor; reverse audio uses captured samples. Branching backward discards future TAS inputs. The rewind budget is bounded (tested at 600 frames / 64 MiB).

**Fast-forward:** Raises emulation playback to about 150% with continuous audio resampling; it is disabled during active TAS recording. Forward-only AVI capture also limits rewind/fast-forward.

**States and movies:** Save-state and host TAS formats are versioned. Current states may not load in older app builds (or vice versa); keep a backup. Do not load a state without necessary recorded-input history while a TAS recording is active.

**Game Genie:** Cheat input and application are frontend-controlled; test each cheat independently and retain the original ROM.

## Supported files and legal data

The core handles supported NES iNES/NES 2.0 and FDS images; the desktop frontends additionally search supported `.zip` containers for loadable images. Other disc, audio and unrecognized image formats may not load. FDS play requires an **8192-byte** `disksys.rom` obtained legally by the user and installed under the per-user `bios/` folder listed in the [build guide](BUILDING.md).

No commercial game ROMs, external test binaries or BIOS images are distributed with the project.

## Saving and capturing

All user-writable files are kept in the platform's per-user data root, not the executable directory. The application creates directories such as `Saves/`, `SaveState/`, `Screenshots/`, `Videos/`, `TAS/`, `Cheats/`, `bios/`, `config/` and `logs/` without erasing their contents. See [platform storage](BUILDING.md#per-user-data-paths).
