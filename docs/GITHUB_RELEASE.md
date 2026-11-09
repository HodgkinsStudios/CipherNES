# GitHub v1.0.0 source release checklist

This tree is staged for a **v1.0.0 release candidate**, not automatically published. An older GitHub release/tag already exists for `1.0.0`; do **not** force-update or replace that tag. Decide whether to publish under a new tag (e.g. an explicitly authorized replacement version) or retain as source-only preparation.

1. Review `VERSION`, `RELEASE_NOTES_1.0.0.md`, `CHANGELOG.md`, legal notices, `.gitignore`, and that any tracked `TEST_ROMS/` fixtures have been cleared for redistribution before making the repository public. Ensure no BIOS/saves/compiled binaries were committed.
2. Run `python3 .github/ci/check_release_version.py`, Ubuntu build, mapper audit, host audio, and timeline tests. Record artifact checksums and exact tree revision.
3. Push a reviewed branch / pull request. Confirm **Ubuntu, Windows, macOS (Intel/ARM), AppImage, Debian and Flatpak** GitHub Actions jobs from the exact source commit (CI files are present, but running this source locally cannot establish their outcome).
4. Review executable UI startup, gamepad configuration, file browser, save/restore, and FDS BIOS path on actual target systems. Document current NMI three-pixel mismatch.
5. Produce release ZIP/AppImage/DEB/Flatpak/Windows/macOS packages only from validated runners; use `VERSION`-driven artifact names and SHA-256 manifests.
6. Explicitly approve the remote tag and release page; **no GitHub publication is performed by this cleanup pass**.

The `TEST_ROMS/` directory is an optional user-supplied fixture set; keep the official release-source exports ROM-free. The source-export workflow explicitly excludes it. For ongoing private development, use [checkpoint rules](CHECKPOINT_WORKFLOW.md).
