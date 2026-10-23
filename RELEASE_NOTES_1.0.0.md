# CipherNES 1.0.0 — native NES/Famicom emulator

**Created by Jacob Hodgkins · HodgkinsStudios**

CipherNES 1.0.0 provides a shared C++ NTSC emulation core and SDL2 native Windows, macOS and Ubuntu frontends.

## Included features

- Gamepad and keyboard remapping, native file choosers, drag-and-drop ROM loading and archive support.
- Save states, rewind with frame-exact restoration and smooth reverse PCM audio, and 150% fast-forward with smooth speed/audio transitions.
- TAS input recording and playback with rewind/save-state branch replacement; fast-forward is disabled while recording TAS.
- Windowed, aspect-correct fullscreen and stretched fullscreen; reserved Escape cycles the modes.
- Game Genie, screenshots, FDS support with user-provided BIOS, configurable APU channels and optional AVI output.
- Trimmed frontend-only core ABI (**v10**), unused editor/debug facilities removed, preserved save-state data format (**v6**).

## Accuracy and verification

The newly trimmed Ubuntu core showed **AccuracyCoin 146/146 (100%)** in a fresh complete ROM run. The external `nes-test-roms` archive previously completed **289/289 valid iNES ROMs**, each executing 600 frames with successful state round trips. These archive results are smoke checks, **not** individual internal test PASS results. See `docs/VALIDATION.md` for verification details and limits.

**Third-party test-ROM suite:** [christopherpow/nes-test-roms](https://github.com/christopherpow/nes-test-roms); its ROMs are not included.

GitHub Actions completed successful Windows and macOS Intel/Apple Silicon builds on October 22, 2026 (commit `a65eee455`); physical-device audio, presentation, and input checks remain outstanding. Linux Flatpak file dialogs have an outstanding portal/sandbox limitation.

## Build and license

Ubuntu: `./build_ubuntu.sh` · Windows: `build_windows.bat` · macOS: `./build_macos.sh`.

MIT licensed; see `LICENSE` and `THIRD_PARTY_NOTICES.md`. No game ROMs or proprietary FDS BIOS are bundled.
