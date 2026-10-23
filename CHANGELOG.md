# Changelog

## 1.0.0 — October 21, 2026

- CipherNES Core and native Windows, macOS and Ubuntu frontends prepared for first public release.
- Smooth real-time reverse rewind sound and frame-accurate reversible timeline with TAS-aware save/load branching.
- Fast-forward target set to 150%, continuous-phase forward audio and smooth speed ramping; remains disabled during TAS recording.
- Escape cycles windowed, aspect-correct fullscreen and stretched fullscreen and cannot be remapped.
- Operating-system ROM/TAS file choosers and native folder dialogs.
- Consolidated core/frontend API: 40 unused exports, orphan PPU editor preview, debug APU write hook, core-side pacing and CPU RAM lock hot-path logic removed. Core ABI **v10**, core state **v6** retained.
- Updated the release documentation and accuracy notes. Preserved 146/146 historical Ubuntu AccuracyCoin evidence while distinguishing ROM execution smoke checks from actual test verdicts.
- Removed development archives, top-level test/videos folders, and generated assets. GitHub CI utilities remain in `.github/ci/` for reproducible validation.
