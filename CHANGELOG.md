# Changelog

## 1.0.1 — October 24, 2026

- Reduced default rewind retention from 1,800 to 600 NTSC frames (about 10 seconds), with a separate 64 MiB capacity-aware heap budget, on Windows/Linux/macOS.
- Corrected a rewind-compressor over-allocation: compressed mostly-static frames no longer preallocate half a raw 240 KiB framebuffer per saved frame.
- Reused the full-sized per-frame state serializer buffer instead of shrinking and zero-filling its unused capacity every frame.
- Kept the original 60.0988138974 FPS NTSC pacing target; preserved the emulator core, savestate format, and mapper behavior.
- Expanded rewind tests for bounded retention, exact state/video restores following eviction, memory-cap pressure, and timeline branches.
- Synced packaging CI artifact names, source export, AppStream metadata, and release notes to 1.0.1.
- This is a performance/stability point update; exact GUI frame pacing remains host- and driver-dependent.


## 1.0.0 — October 21, 2026

- CipherNES Core and native Windows, macOS and Ubuntu frontends prepared for first public release.
- Smooth real-time reverse rewind sound and frame-accurate reversible timeline with TAS-aware save/load branching.
- Fast-forward target set to 150%, continuous-phase forward audio and smooth speed ramping; remains disabled during TAS recording.
- Escape cycles windowed, aspect-correct fullscreen and stretched fullscreen and cannot be remapped.
- Operating-system ROM/TAS file choosers and native folder dialogs.
- Consolidated core/frontend API: 40 unused exports, orphan PPU editor preview, debug APU write hook, core-side pacing and CPU RAM lock hot-path logic removed. Core ABI **v10**, core state **v6** retained.
- Updated the release documentation and accuracy notes. Preserved 146/146 historical Ubuntu AccuracyCoin evidence while distinguishing ROM execution smoke checks from actual test verdicts.
- Removed development archives, top-level test/videos folders, and generated assets. GitHub CI utilities remain in `.github/ci/` for reproducible validation.
