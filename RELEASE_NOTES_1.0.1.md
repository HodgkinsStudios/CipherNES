# CipherNES 1.0.1 — performance and rewind stabilization

**Development checkpoint CP01, October 24, 2026 — not yet a published GitHub release**

## Improvements

- Ten-second rewind: 600 frame states at 60.0988138974 NTSC FPS (up to about 9.97 seconds). Older states are evicted automatically.
- Independent 64 MiB rewind allocation budget. Capacity is accounted instead of just compressed payload length.
- Reduced per-frame memory amplification from a 50%-of-raw-size compressed output reservation. Sparse video deltas no longer each reserve 120 KiB when a far smaller compressed packet sequence will do.
- State serializer staging scratch stays allocated for subsequent frames. The encoded length is passed directly into history append without shrink/grow/zero-fill overhead.
- Rewind implementation updated in both Ubuntu/macOS-shared and Windows frontend. Emulator timing target, save format, CPU/PPU/APU implementation, and mapper behavior remain unchanged; core version display now identifies 1.0.1 with API ABI v10 and state v6 retained.

## Test evidence

- Ubuntu release-mode build succeeded.
- Timeline regression including 600-frame eviction, exact reverse state and picture, branch truncation, and 64 MiB pressure passed.
- TAS branch self-test passed in headless SDL mode.
- Supplied AccuracyCoin.nes validated for 600 frames, with framebuffer CRC32 `0624745B` and save-state round-trip PASS. This does **not** prove all 146 AccuracyCoin internal checks pass; past evidence for 146/146 is historical only.
- 289/289 valid iNES tests in the supplied corpus loaded, executed 20 frames, and state-round-tripped successfully; one invalid-header .NES was excluded. All resulting framebuffer CRC32 values were identical to the v1.0.0 baseline for the same 20-frame input. This is a **smoke test**, not automated reading of each test ROM's PASS/FAIL verdict.
- 600-frame AccuracyCoin full rewind workload ~230-240 frames/second on the test host, >3.8x real time. Benchmark variation does not justify claiming universal 100% GUI speed; graphical desktop/audio drivers and long-duration load still need checking.

## Known limitation / next step

- Linux GUI fullscreen pacing and user-specific hardware cannot be certified in this headless environment. On the user's Ubuntu system, run with `CIPHERNES_TIMING_DIAGNOSTICS=1`, test at 100% speed for 20 minutes including fullscreen and sound, and capture pacing logs to identify any intermittent host stalls.
- Windows and macOS frontends share the same rewind changes but cross-platform binaries were not produced in this Ubuntu-only run.
- Avoid copying test ROMs into public GitHub releases.

## Packaging and documentation fixes in release candidate

- Synchronized Debian, Flatpak, AppImage, source-export workflows and desktop/AppStream metadata to version 1.0.1.
- Updated Windows and Ubuntu release packagers to use matching 1.0.1 notes.
- Added CI version/reference consistency gate. Windows and macOS packaging builds must pass in GitHub Actions before attaching final cross-platform assets.
- Existing 1.0.0 downloads are intentionally linked as previous releases until the 1.0.1 release is actually published.
