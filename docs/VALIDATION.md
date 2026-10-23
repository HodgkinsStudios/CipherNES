# CipherNES 1.0 accuracy and validation

**Maintained by HodgkinsStudios**

## What the numbers mean

- **AccuracyCoin:** The trimmed Ubuntu 1.0 core ran the supplied updated `AccuracyCoin.nes` and its final framebuffer displayed **TESTS PASSED: 146 / 146 (100%)**. This was a full AccuracyCoin test outcome, not merely a boot check.
- **NES test-ROM archive:** The supplied `nes-test-roms-master(1).zip` previously recorded **289 / 289 valid iNES images** booting, executing 600 frames and passing a core-state save/load round trip. An additional non-iNES entry was excluded. These are **smoke/execution checks**, not independently inspected internal test verdicts.
- Historical named CPU/APU/MMC3/PPU result-based testing is documented in [`NTSC_ACCURACY.md`](NTSC_ACCURACY.md). Some profiles require specific hardware settings, notably the Mapper 4 alternate IRQ revision.

**Test ROM source and credit:** [christopherpow/nes-test-roms](https://github.com/christopherpow/nes-test-roms). Individual test ROMs retain their original authorship and license terms; CipherNES bundles none of them.

## Reproducing tests without adding ROMs to the release

From the repository root on Ubuntu:

```bash
./build_ubuntu.sh
python3 .github/ci/make_synthetic_rom.py /tmp/ciphernes-selfcheck.nes
./.github/ci/test_timeline.sh /tmp/ciphernes-selfcheck.nes
./.github/ci/test_host_audio.sh
CIPHERNES_RUNTIME_DIR=/tmp/ciphernes-tas-ci SDL_AUDIODRIVER=dummy xvfb-run -a \
  ./build/ubuntu/bin/CipherNES --tas-branch-test /tmp/ciphernes-selfcheck.nes
# Supply the external suite locally; the source archive contains no ROMs:
python3 .github/ci/run_ntsc_corpus.py /path/to/nes-test-roms-master.zip --frames 600 --jobs 4
```

The 1.0 cleanup was followed by **289 / 289** passing 600-frame NTSC archive load/execute/save-restore checks, **146 / 146** visible AccuracyCoin passes, and reversible frame/TAS/audio regression passes. The corpus runner checks load/execute/state restoration, never internal PASS assertions. A full accuracy certification requires inspecting each ROM's defined pass channel/display and running for sufficient time with the appropriate hardware profile.

## Release boundaries

The shared C core retains state **version 6** and the required CPU/PPU/APU/mapper timing and save-state paths. The cleanup changes the API symbol surface from version 9 to **10**. The October 22, 2026 GitHub Actions workflows built Windows and both macOS architectures successfully (commit `a65eee455`). Native physical-device presentation, controller, and audio behavior still requires hardware verification. Flatpak native dialogs are separately limited by sandbox integration.
