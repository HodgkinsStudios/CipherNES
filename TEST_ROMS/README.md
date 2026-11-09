# TEST_ROMS — user-supplied diagnostic fixtures

This folder contains **the exact test-ROM images supplied for CipherNES v1.0.0**.
It is intentionally separate from the emulator's implementation and regular
built release files. The `nes-test-roms` subtree preserves the ZIP's category
structure, and `AccuracyCoin.nes` / `AccuracyCoin.asm` are copied byte-for-byte
from the separately supplied files.

## Inventory

- **291 `.nes` files**: 290 from the NES test-suite ZIP plus 1 AccuracyCoin ROM.
- **290 valid iNES headers**: 289 in the suite and AccuracyCoin. The supplied
  `nes-test-roms/other/LINUSMUS.NES` has no valid iNES header, so it is kept
  intact but excluded from iNES smoke runs.
- **2 `.fds`**, **13 `.nsf`**, and **3 `.unif`** images from the source suite.
  These are retained but are **not covered by the `.nes` smoke runner**.
- Supplied `test_roms.xml`, suite readmes, the `nestest.log` CPU oracle, and
  AccuracyCoin's `.asm` are retained as reference material.
- `ROM_MANIFEST.csv` records each image's SHA-256, size, and iNES status.

## Run checks locally or in GitHub Actions

```bash
python3 .github/ci/verify_test_roms.py
CIPHERNES_LTO=0 CIPHERNES_JOBS=4 ./build_ubuntu.sh
python3 .github/ci/run_ntsc_baseline.py TEST_ROMS/nes-test-roms --frames 600 --jobs 4
./tests/run_accuracycoin.sh TEST_ROMS/AccuracyCoin.nes /tmp/accuracycoin-final.ppm
```

In GitHub, open **Actions > Test ROM corpus (manual) > Run workflow**.
The workflow verifies SHA-256 hashes, builds the emulator, and runs a 600-frame
NTSC execution/save-state baseline on the 274 NTSC images. The 15 PAL-specific
ROMs are excluded from this NTSC baseline; one invalid iNES is also excluded.
Complete per-ROM results and framebuffer CRC32 values are exported to JSON and CSV. An optional setting executes AccuracyCoin's
full test mode and saves the screen for visual review.

**Interpretation:** Smoke-test success means only that the ROM loaded,
executed the requested frames, and restored a save state. It is **not** the
ROM's internal accuracy PASS result. AccuracyCoin's final PPM needs its
printed score verified; an exit code alone is not its 147/147 score.

## Redistribution and repository visibility

These third-party test ROMs are user supplied. Their inclusion in this work
copy **does not convey redistribution permission**. The original test suite
contains files with different provenance and licensing. Verify applicable
authors' permissions and the rights for each test image **before pushing a
ROM-inclusive project to a public GitHub repository**. A private repository
or a local-only test-data directory may be more suitable. The clean ROM-free
release-source checkpoint remains separate.

No ROM in this folder is compiled into the CipherNES core or desktop packages.

## NTSC baseline evidence

See [`docs/NTSC_TEST_ROM_BASELINE.md`](../docs/NTSC_TEST_ROM_BASELINE.md) for the
measured release baseline, explicit grading scope, known limitations, and
[`tests/results/NTSC_600_FRAME_BASELINE.csv`](../tests/results/NTSC_600_FRAME_BASELINE.csv)
for each of the 274 NTSC images. The smoke runner does not judge a visually
subjective test or pretend an on-screen PASS is known merely from execution.
