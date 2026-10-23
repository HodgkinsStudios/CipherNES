# CipherNES 1.0.0 NTSC Compatibility

CipherNES 1.0.0 targets NTSC NES/Famicom timing.

For details, the compatibility notes are split into two pages:

- [NTSC Mapper Compatibility](NTSC_MAPPERS.md) — the complete supported mapper-ID list and mapper-family notes.
- [NTSC Accuracy Certification](NTSC_ACCURACY.md) — the certified release scores, AccuracyCoin highlight, named formal test-ROM results, profile-specific cases, and historical diagnostic notes.

## Summary

The trimmed Ubuntu core passed the updated AccuracyCoin suite **146/146**. A separate 289/289 NTSC ROM execution/state round-trip audit passed; it is not an internal per-ROM accuracy certification. Windows and macOS GitHub Actions builds passed on October 22, 2026 (commit `a65eee455`); physical-hardware acceptance checks remain outstanding.

> **Earlier NTSC certification (September 4, 2026):** The machine-verifiable release corpus passed, with profile-specific mapper tests run against their intended hardware models. The previous NTSC group totals remain historical evidence rather than a new full rerun after the repair.

The certification run was completed on **September 4, 2026**. The later documentation cleanup only reorganized those recorded results; it did not rerun the external test ROMs.

## Scope

For CipherNES 1.0.0, the NTSC compatibility claim means:

- the maintained machine-verifiable NTSC release corpus passed;
- the updated AccuracyCoin ROM passed **146 / 146 (100%)** on the trimmed Ubuntu core during the 1.0 release audit;
- profile-specific mapper tests passed under the silicon model they intentionally target;
- the October 21 supplied NTSC test archive completed **289 / 289 valid iNES ROMs** loading, executing for 600 frames and passing a state round-trip (this does not guarantee 289 internal test verdicts); the prior historical tree recorded 270/270 load/execute;
- no known failing machine-verifiable NTSC accuracy test remained in the certified release corpus.

These results describe the tested release corpus. A future test ROM, unusual cartridge, undocumented peripheral, malformed dump, or newly discovered hardware behavior could still uncover a bug.

## Test-ROM distribution

Third-party validation ROM binaries are **not** distributed in this repository. The public docs preserve the certification record and test names so results remain auditable without redistributing the ROMs.
