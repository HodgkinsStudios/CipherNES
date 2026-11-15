# Development, source hygiene and checkpoints

## Source-of-truth rules

- Start only from the latest **independently verified authoritative** development checkpoint. Confirm the full archive filename and SHA-256 before modifying source; never blend checkpoints or assume an older archive is current.
- Maintain one portable native core and separate Linux, Windows and macOS frontends. Core or mapper changes require targeted hardware evidence and a regression rerun.
- Keep generated executable/library files, scratch screenshots, ROMs, saves and detailed per-pass results out of the main source tree. A clean ROM-free checkout should build with the documented scripts.
- Classify test ROMs as internal PASS, internal FAIL, functional demonstration PASS, blocked/unsupported, or UNVERIFIED according to actual evidence. Smoke execution is not a hardware verdict.

## Permanent Google Drive checkpoint policy

**Project root:** https://drive.google.com/drive/folders/1kJnie_Wkxz6vs0MaNwMD_SpoatO6rI6E

1. Give every completed development pass its own `CP###_YYYY-MM-DD_HHMM_EST_OR_EDT_DESCRIPTION` folder. Use the real America/Toronto time and never overwrite earlier folders.
2. Upload complete, ROM-free source; SHA-256 checksums; a separate changes/validation report; a handoff note; and an exact, copy-pasteable next-checkpoint continuation prompt.
3. Split any individual artifact over **50,000,000 bytes (decimal)** into numbered parts ≤50,000,000 bytes, including per-part hashes and reassembly instructions. `tools/checkpoints/create_checkpoint.py` and `tools/checkpoints/reassemble_source.py` are retained for this workflow.
4. Read back every folder listing. Re-download the source ZIP or numbered parts, reconstruct as needed and independently check the expected SHA-256.
5. Never store commercial ROM dumps, private BIOS images, runtime saves, screenshot-corpus evidence, generated builds or credentials in a ROM-free source archive. External *testing evidence* may be stored separately in the private checkpoint folder where permitted.
6. Record only tests actually run and mark all unavailable fixture/toolchain checks accurately. Avoid unsupported compatibility or accuracy claims.

## Paused survey

The old all-mapper ROMDUMP title/menu screenshot survey is **paused**. Its images, ledgers and survey-only tools are intentionally absent. Do not restart or reconstruct them unless explicitly authorized. This does not prohibit ordinary, separately authorized NES hardware regression testing.
