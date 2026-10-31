# CipherNES checkpoint and handoff rule

**Always preserve this procedure across conversations and code handoffs.**

1. Upload every new CipherNES checkpoint to Google Drive in the existing top-level `CipherNES` folder.
2. Give **every** checkpoint a separate timestamped subfolder, named with UTC ISO time and a descriptive checkpoint identifier. Never overwrite earlier folders or their archives.
3. Include the complete source (excluding copyrighted test ROMs, builds, and generated runtime artifacts), a tested Ubuntu build when available, SHA-256 manifest, validation reports, and a continuation prompt.
4. If any checkpoint ZIP is larger than 50 MB, split it into chunks **each <= 50 MB** and document how to reassemble them. Preserve the original source tree layout within the reconstructed archive.
5. Carry this Google Drive rule forward in `docs/CHECKPOINT_WORKFLOW.md` AND every continuation/handoff prompt.
6. Do not state that a test ROM suite has passed its internal accuracy assertions based only on successful `--validate` smoke checks. Preserve baseline and post-change results separately.

Current v1.0.1 CP01 destination:
`CipherNES/2026-10-24T20-53-00Z_CipherNES_v1.0.1_Optimization_CP01/`

Source of truth for this checkpoint is the source archive embedded in that folder. Keep user's local clone intact and review diffs before Git commits.

## v1.0.1 GitHub release preparation

The post-CP01 release cleanup and CI version synchronization belong in a NEW timestamped
Google Drive checkpoint. After a validated GitHub push, record the resulting commit SHA
and CI status here or in the continuation prompt. Never silently replace CP01.
