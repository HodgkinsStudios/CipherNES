# External NES test fixtures (not distributed)

No NES/FDS/NSF/UNIF test images, game ROMs, Famicom Disk System firmware, upstream source archives, or result screenshots are included in CipherNES.

`ROM_MANIFEST.csv` lists the owner-supplied test corpus used during development. It is a **reference/checksum manifest**, not a statement that these files ship with the project. Restore your authorized test images under `TEST_ROMS/` at their manifest-relative paths when needed. The original upstream test suite documentation can be obtained from its respective upstream project; it is not redistributed in this source tree.

```bash
python3 .github/ci/verify_test_roms.py
python3 .github/ci/run_ntsc_baseline.py TEST_ROMS/nes-test-roms --frames 600 --jobs 4
./tests/run_accuracycoin.sh TEST_ROMS/AccuracyCoin.nes /tmp/ciphernes_accuracycoin.ppm
```

The NTSC runner checks execution and state restoration; it **does not** grade the internal hardware verdicts. The AccuracyCoin output image must be read and its displayed score verified separately. See [testing and limitations](../docs/TESTING.md).

Upstream test-project credits: [NES test-ROM collection](https://github.com/christopherpow/nes-test-roms) and [AccuracyCoin](https://github.com/100thCoin/AccuracyCoin). Original author and license notices apply. For measured results and limitations, see [testing](../docs/TESTING.md).
