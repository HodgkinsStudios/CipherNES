# CipherNES v1.0.1 CP01 — continuation prompt

Continue the user's CipherNES (C++, SDL2) emulator **from the attached CP01 full-source checkpoint**, not from an older v1.0 release. The user has decided to prepare a v1.01 optimization update and has a local clone. Do not publish a GitHub release or alter the user's local clone without explicit instructions.

**Completed CP01**
- Configured rewind to retain at most 600 snapshots (~10 NTSC seconds) and 64 MiB allocated rewind history on Linux/macOS and Windows.
- Fixed severe over-reserving of video-diff compression; memory budget now counts allocated vector capacities.
- Eliminated serializer scratch shrink/regrow on each emulated frame by adding pointer+size append overload.
- Version bumped to 1.0.1 (core display, VERSION, CHANGELOG; ABI v10 and state v6 unchanged).
- Built optimized Ubuntu executable (`build_ubuntu.sh`).
- Rewind, TAS, and 289/289 valid iNES *smoke* checks passed; 0 checksum differences against baseline in 20-frame deterministic runs, 1 invalid ROM skipped. AccuracyCoin 600-frame CLI round trip passed, CRC 0624745B. **Do not call these internal accuracy PASS assertions.**
- This point update leaves CPU/PPU/APU, mappers, video timings, state serialization format intact. Normal NTSC speed target 60.0988138974 FPS.

**Pending work**
- Obtain actual native Ubuntu fullscreen/pacing diagnostics from 15-20 minute real gameplay, especially SDL GPU vsync and audio stalls. Do NOT promise physically guaranteed exact 100% speed based solely on headless benchmark.
- Optionally run longer, ROM-specific accuracy tests by detecting each ROM's advertised verdict rather than treating `--validate` as accuracy certification.
- Build and test Windows/macOS binaries in native runners before public v1.0.1 publication. Update GitHub release packaging only after verification.
- Ask user for priority on future memory viewer/editor and CPU debugger, which were discussed as a separate feature planning effort; do not silently add complex debug frontends to this narrow optimization update.

**Mandatory Google Drive rule:** Every future CipherNES checkpoint goes to top-level `CipherNES` folder on Google Drive, in a *new timestamped subfolder*. Never replace an earlier checkpoint. Include complete source, tested build(s), test/benchmark notes, and handoff prompt. Split any ZIP over 50 MB into parts <=50 MB and document reassembly. Preserve this instruction in `docs/CHECKPOINT_WORKFLOW.md` and the next continuation prompt.

CP01 folder: `CipherNES/2026-10-24T20-53-00Z_CipherNES_v1.0.1_Optimization_CP01/`.

**Local-clone merge on Ubuntu:** Extract CP01 source archive and review its diff against your local clone; for example `rsync -av --exclude=.git --exclude=build CipherNES-1.0.1/ ~/path/to/CipherNES/`; then `git diff`, `./build_ubuntu.sh`, commit normally. Do not overwrite `.git` or untracked custom files.
