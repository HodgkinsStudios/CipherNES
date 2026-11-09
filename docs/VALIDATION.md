# Accuracy & validation (v1.0.0 source candidate)

Separate **test framework execution** from **test ROM internal PASS verdicts**. The AccuracyCoin, mapper, audio, rewind and synthetic validation checks were **freshly rerun** on the cleaned v1.0.0 source. Extended external-suite totals are retained from CP021 history.

| Check | Historical evidence | Meaning |
| --- | --- | --- |
| AccuracyCoin combined suite | **147 / 147 PASS (fresh v1.0.0 build)** | Final result screen inspected; 4,159 frames; save-state roundtrip PASS |
| Synthetic mapper audit | **156 / 156 mapper ID smoke, 7,156 / 7,156 assertions (fresh)** | Register/board feature checks, not comprehensive game validation |
| External `nes-test-roms` ZIP | 289 / 289 valid ROMs ran for 600 frames + restored state | Execution/savestate only; not 289 internal PASS verdicts |
| Archived individually graded ROMs | 175 documented PASS | Results collected across prior checkpoints, not all rerun on one build |
| Remaining known mismatch | `nmi_sync/demo_ntsc.nes` 3-pixel stripe discrepancy | Pending PPU timing correction; not PASS |

**Limitations:** PAL-only ROMs are excluded from NTSC claims; external test suites may require controls, peripherals, manufacturer-specific profiles, screenshots, or audio traces. MMC5 fine raster/audio accuracy is not fully certified; 156 registered mappers is not a full-accuracy promise.

## Reproduce build and ROM-free checks (Ubuntu)

```sh
CIPHERNES_LTO=0 CIPHERNES_JOBS=4 ./build_ubuntu.sh
./tests/run_mapper_audit.sh
./.github/ci/test_host_audio.sh
python3 .github/ci/make_synthetic_rom.py /tmp/ciphernes-synthetic.nes
./.github/ci/test_timeline.sh /tmp/ciphernes-synthetic.nes
./build/ubuntu/bin/CipherNES --validate /tmp/ciphernes-synthetic.nes 180
python3 .github/ci/check_release_version.py
# Run user-supplied AccuracyCoin ROM in TEST_ROMS (review rights before public push):
./tests/run_accuracycoin.sh TEST_ROMS/AccuracyCoin.nes /tmp/accuracycoin-final.ppm
# Inspect the final .ppm for TESTS PASSED: 147 / 147.
```

For this developer/test checkout, external ROMs are stored intentionally in `TEST_ROMS/` and can be tracked by Git after checking third-party redistribution rights. Normal public release packages should remain ROM-free unless rights have been confirmed. For archived detailed grading logs, consult the CP021 Google Drive checkpoint, not the public source ZIP: https://drive.google.com/drive/folders/1gO9aeD8zz10UOoWdVbQjw0OogOCEB64A .

## Optional bundled ROM test corpus

See [`TEST_ROMS/README.md`](../TEST_ROMS/README.md), `.github/ci/verify_test_roms.py`, and the manual **Test ROM corpus** workflow. The ROM corpus runner accepts either a ZIP file or an extracted directory. It only smoke-tests valid iNES images, not a full internal pass/fail grade.

## NTSC test-ROM corpus

The measured, per-ROM NTSC test corpus baseline is documented in
[`NTSC_TEST_ROM_BASELINE.md`](NTSC_TEST_ROM_BASELINE.md). The 600-frame boot,
execution and state-restoration result is distinct from a ROM-defined internal
PASS/FAIL verdict; no unsupported accuracy percentages are inferred from
execution alone. Reproduce it with `.github/ci/run_ntsc_baseline.py`.
