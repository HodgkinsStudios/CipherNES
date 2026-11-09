# Cartridge mapper coverage (v1.0.0)

The CP021 core has **156 registered mapper/disk IDs**, verified by the synthetic smoke registry in `tests/mapper_audit_regression.cpp`. This count represents mapper factory recognition, **not** fully accurate boards or game-by-game compatibility. Focused feature coverage currently exists for a subset (historically about 34 IDs); some clone submappers and analog/audio/raster behaviors remain incomplete.

Common supported families include NROM (mapper 0, including NROM-368), MMC1/SxROM (1), UxROM (2), CNROM (3), MMC3/MMC6 (4), MMC5 (5), AxROM (7), MMC2 (9), MMC4 (10), VRC6 (24/26), MMC3 nametable/CHR variants (118/119), and many additional ASIC, clone, and multicart boards. Full source-based factory assignments are in `CipherNES_Core/cartridge.cpp` and `CipherNES_Core/Mappers/`.

**Known open items:** Fine MMC5 split-fetch/audio analog accuracy; rare board hardware profiles; partial coverage of unverified mapper IDs; three-pixel NMI synchronization visual mismatch in an external PPU test. Silicon-specific MMC3 IRQ tests must be run with their documented early/late revision profile.

Run `./tests/run_mapper_audit.sh` on Ubuntu for per-ID smoke checks and focused hardware regression assertions. Do not generalize a passing mapper smoke test into a game-compatibility claim.
