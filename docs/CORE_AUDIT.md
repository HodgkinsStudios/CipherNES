# Core/front-end feature audit — CipherNES 1.0

**Maintained by HodgkinsStudios**

Analyzed the `CIPHERNES_API` exports against all desktop frontend call sites. **69** originally exposed C functions; **28** had direct frontend calls. An additional Mapper 4 IRQ silicon diagnostic entrypoint is retained to preserve hardware-profile accuracy investigation. **40 unused exports were removed; 29 remain.**

| Core capability | Used by frontend? | Action |
| --- | --- | --- |
| Cartridge load/unload, reset, FDS disk controls | Yes | Retained |
| Frame-by-frame emulation, gamepad state, framebuffer access | Yes | Retained |
| PCM i16 samples, output sample rate, audio controls | Yes | Retained; smooth sound unaffected |
| Transient/persistent states, state size and restoration | Yes | Retained; rewind and TAS depend on these |
| Game Genie codes, channel muting | Yes | Retained |
| MMC3 alternate IRQ hardware revision | Special accuracy test | Retained for regression verification |
| Core-side frame pacing/turbo, multi-frame batching/counters | No | Removed; host controls pacing |
| PPU background editor preview/video-output suppression | No | Removed; all actual frames still render |
| Float/sample-vector audio variants, debug APU register logger | No | Removed; signed PCM and live APU kept |
| Core toast fields, memory peek/poke/copy/debug dump | No | Removed exported APIs; legacy state payload remains parseable |
| CPU RAM lock interface and hot-path lookup | No | Removed; reserved state field kept for binary compatibility |

A removed C API symbol does not necessarily translate to a proportional runtime speed increase: unused functions already incur little per-frame overhead. The meaningful hot-path reductions are RAM-lock branches/lookups, optional APU register-write logging, and the optional framebuffer-write check. These were removed without changing the accuracy-sensitive hardware execution paths.

Shared `CipherNES_Core/` source is used for Linux, Windows and macOS builds. The macOS program delegates to the shared Unix frontend, and Windows uses its corresponding SDL frontend. Both link the same versioned C API. Windows and macOS GitHub Actions builds subsequently passed on October 22, 2026 (commit `a65eee455`); runtime behavior on physical devices remains to be verified.
