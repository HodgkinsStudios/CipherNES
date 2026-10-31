# CipherNES Core 1.0 — frontend-focused C interface

**Maintained by HodgkinsStudios**

`CipherNES_Core/ciphernes_core.h` defines the public C ABI used by all three desktop frontends. The 1.0 cleanup reduced the public exports from **69 to 29** by removing unused editor, inspection, host pacing, and memory-lock interfaces. **API version 10** deliberately identifies the revised symbol set; **save-state version 6 is unchanged**.

The remaining operations include lifecycle/version reporting, ROM image/path loading and unloading, soft/hard reset, FDS inspection/eject/next-side, deterministic per-frame execution and controller writes, ARGB video access, output sample rate and signed 16-bit PCM pulls, audio enable/channel mask, save-state sizing, persistent/transient snapshots, and snapshot restoration. The Mapper 4 alternate IRQ silicon selection entrypoint is retained for hardware-profile accuracy verification even though it is not exposed through the desktop menu.

The core does not pace frames, drop audio during fast-forward, or manage fullscreen. These operations are owned by the native frontend. Full CPU, PPU, APU and mapper timing and every emulated frame remain intact for accuracy, TAS and smooth rewind. APU register-write debug callbacks, unused standalone PPU preview rendering, float/audio-vector delivery variants, and CPU RAM lock processing were removed. State version 6 still reads legacy RAM-lock entries but ignores them, and writes an empty count to retain the binary layout. Previously configured RAM locks are not retained.

The source and library are built together. Old third-party clients that called removed exports must adapt to API v10; this cleanup is not binary-compatible with the previous API v9. Using a matching frontend/core pair is mandatory.
