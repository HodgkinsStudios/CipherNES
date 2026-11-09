# Development checkpoint procedure (not release source history)

For ongoing CipherNES development, retain the authoritative full-source checkpoints in the existing [CipherNES Google Drive folder](https://drive.google.com/drive/folders/1kJnie_Wkxz6vs0MaNwMD_SpoatO6rI6E).

1. For each completed development pass, create a separate timestamped folder. Never overwrite an earlier checkpoint.
2. Include a complete ROM-free source ZIP, a SHA-256 manifest, source reassembly script, concise change/test report, evidence logs, and separate copy-and-paste continuation instructions specifying the exact authoritative checkpoint and checksum.
3. If an archive exceeds **50,000,000 bytes**, split it into numbered chunks no larger than 50,000,000 bytes and verify the assembled SHA-256.
4. Preserve ROMs, BIOS firmware, save files, screenshots, compiled binaries, and temporary test outputs **outside** the published source checkpoint.
5. On each future handoff, carry the checkpoint rule, current authoritative checkpoint folder, hash, and outstanding limitations forward.
6. Never confuse successful execution/state roundtrips with a test ROM's internal PASS verdict; list both separately.

This public v1.0.0 release tree intentionally omits historical CP reports and private grading fixtures. They remain in the immutable external checkpoints. Version 1.0.0 is a **source/release-label choice**, not a claim that every mapper or game is fully accurate.
