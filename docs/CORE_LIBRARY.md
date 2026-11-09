# CipherNES native core interface

The shared C interface used by Linux, Windows and macOS frontends is declared in `CipherNES_Core/ciphernes_core.h`. The implementation remains in the C++ native core.

Call `ciphernes_get_api_version()` at runtime and compare with `CIPHERNES_API_VERSION` from the matching header; **CP021 uses API version 11**. Keep the frontend and its core built from the same source snapshot.

The interface provides lifecycle, cartridge/FDS load and reset, per-frame stepping with controller input, framebuffer and PCM access, input and audio configuration, and save/load state access. Rewind history, timing/pacing and host UI remain frontend responsibilities.

**State compatibility:** Save formats have evolved in older releases. Users should retain backups and must not assume states from older publicly tagged 1.0.0/1.0.1 versions can be loaded by this source candidate.

This document is a high-level orientation. Use the checked-in header and generated builds as the definitive ABI specification.
