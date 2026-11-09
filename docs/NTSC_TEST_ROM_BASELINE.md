# NTSC test-ROM baseline — CipherNES 1.0.0

This is a **fresh Ubuntu headless run** of the NTSC `.nes` test images in the uploaded `nes-test-roms-master` suite. The emulator implementation is unchanged; all timings shown here are the current v1.0.0 build.

## Inputs and test scope

| Inventory category | Count |
| --- | ---: |
| All `.nes` entries in suite | 290 |
| **NTSC `.nes` files executed** | **274** |
| PAL-only ROMs excluded | 15 |
| File with invalid iNES header excluded | 1 |
| FDS / NSF / UNIF images (different formats; not in this `.nes` run) | 2 / 13 / 3 |

ROM content was checked against the [locked per-ROM SHA-256 manifest](../tests/ntsc_rom_scope.csv). The exact uploaded suite ZIP SHA-256 is `4ce4c06c5c52403079f26179e2f7cb1deb76ce8a10abe65f60ad875dead15875`. The AccuracyCoin ROM is **separate** from this 274-ROM suite and has its own 147/147 result-screen verification in the [README](../README.md).

## Measured 600-frame execution and save-state results

| Test | Result |
| --- | --- |
| **NTSC ROMs executing 600 frames and restoring state** | **274 / 274 PASS** |
| Executions or state roundtrips failing | **0** |
| Total requested emulated frames | 164,400 |
| Per-ROM framebuffer CRC32 | Saved in CSV and JSON |
| Core version | 1.0.0 (Ubuntu) |
| Tested binary SHA-256 | `cb0e5c024a5bb30f2b20142d68691edc648718b4d6677b4c43428a28679e924c` |

Failures, if any: None observed.

Each ROM was loaded through the normal headless `--validate` path, stepped for **600 frames**, and checked for a successful state save/load roundtrip. The resulting framebuffer CRC32 was recorded for future comparisons. The run did **not** inject controller input, validate visual/audio reference output, or automatically parse every ROM's guest-defined pass/fail oracle.

## Internal accuracy verdicts — distinct from boot success

A separately maintained source-backed verdict inventory, generated during the emulator's accuracy investigation, records:

| Internal verdict classification (previously graded; **not rerun by this smoke pass**) | NTSC ROMs |
| --- | ---: |
| Explicit internal PASS | **175** |
| Previously observed visual timing mismatch | **1** |
| Other ROMs without a definitive whole-ROM PASS/FAIL | **98** |
| **Total** | **274** |

The documented visual mismatch is `nmi_sync/demo_ntsc.nes`: the middle alignment line remains approximately **three pixels** displaced against its reference. This is not recast as a PASS by successful emulation. The historical MMC1-A12 result was reclassified as an interactive diagnostic after a false frame-boundary deadlock inference; it is not a whole-ROM PASS.

**Never interpret 274/274 smoke success as 274/274 hardware-accuracy success.** Audio, video, manually driven input, revision-specific mapper diagnostics, and some interactive demos need separate oracles. The previous explicit PASS count is presented as historical evidence, not falsely attributed to this run.

## Reproduce locally or in GitHub Actions

```bash
python3 .github/ci/verify_test_roms.py
CIPHERNES_LTO=0 CIPHERNES_JOBS=4 ./build_ubuntu.sh
python3 .github/ci/run_ntsc_baseline.py TEST_ROMS/nes-test-roms \
  --frames 600 --jobs 4 --timeout 180 \
  --output build/test_results/ntsc_baseline.json
```

The command saves per-ROM JSON and CSV results; the GitHub Actions **Test ROM corpus (manual)** workflow uses this same script and uploads the results as an artifact. Results in this release are preserved in:

- [`tests/results/NTSC_600_FRAME_BASELINE.csv`](../tests/results/NTSC_600_FRAME_BASELINE.csv): 274 source-relative ROM paths, SHA-256, mapper IDs, frame CRC32, run/save-state statuses and historical grading labels
- [`tests/results/NTSC_600_FRAME_BASELINE.json`](../tests/results/NTSC_600_FRAME_BASELINE.json): scope, totals, binary/ROM manifest hashes and full per-ROM data

This is a reproducible baseline for detecting future regressions, **not** a certification that every NTSC cartridge behavior is correct. The suite was supplied by the project owner; test ROM licensing must be evaluated separately before any public redistribution.
