# Core architecture and mapper support

## Boundaries

```text
Linux (SDL2)  ─┐
Windows (Win32/SDL2) ───► CipherNES_Core (native C++) ───► frame/audio/state
macOS (Cocoa/SDL2) ─┘          │
                               └─ CPU · PPU · APU · bus · DMA · cartridge · FDS
```

`CipherNES_Core/ciphernes_core.h` declares the C-compatible public interface; `ciphernes_get_api_version()` should match the header's `CIPHERNES_API_VERSION` (currently **11**). The ABI exposes create/destroy, cartridge and disk loading, frame and clock stepping with controller bytes, framebuffer/PCM access, audio settings, resets and state serialization. The core is compiled into each platform's executable. A dynamic shared-library distribution is not implied by this structure.

Frontends own SDL2 display/audio devices, frame pacing, file dialogs, user controllers, configuration, screenshots, rewind/fast-forward, TAS management, capture and save-file UI. They must not duplicate hardware timing rules.

## Hardware components

- **CPU (Ricoh 2A03):** 6502-family execution, page crossings, interrupt/reset sequencing, unofficial instruction behavior, dummy bus transactions and DMA-related stalls.
- **PPU (Ricoh 2C02):** 256×240 ARGB32 output, scanline/dot timing, background/sprite fetches, VRAM scrolling, palette memory, sprite-zero hit/overflow, register side effects, VBL/NMI and open bus.
- **APU:** Two pulse channels, triangle, noise and DMC with frame counter, envelopes, length/sweep controls, sample generation and DMC/IRQ bus interactions.
- **Bus and DMA:** Internal RAM mirrors, CPU/PPU/APU registers, controller serial reads, OAM/DMC DMA arbitration and mapper-visible address activity.
- **Cartridge:** iNES and NES 2.0 parsing, PRG/CHR ROM/RAM, mirroring, battery-backed RAM, mapper/submapper detection, optional FDS disks and BIOS integration.

The core uses an NTSC-oriented master-clock model, not host frame duration, for hardware ordering. Timing-sensitive behavior is an ongoing accuracy target; no general statement of complete cycle perfection is implied.

## Mapper system

Mapper implementations live in `CipherNES_Core/Mappers/`, registered by the cartridge factory in `CipherNES_Core/cartridge.cpp`. **156 mapper/disk IDs are registered** in synthetic smoke checks; this is not equivalent to 156 fully accurate boards.

Examples include NROM (0), MMC1/SxROM (1), UxROM (2), CNROM (3), MMC3/MMC6 (4), MMC5 (5), AxROM (7), MMC2 (9), MMC4 (10), VRC6 (24/26), and additional clone/multicart hardware. Supported board paths can model PRG/CHR bank maps, PRG RAM, mirroring, IRQ counters, expansion audio, scanline/fetch-visible behavior and per-mapper persistent state.

**MMC3 hardware variants:** IRQ behavior differs across early/late chip revisions. Select the correct emulator revision profile when running silicon-specific tests; a mismatch of test and profile is not automatically an emulator bug.

### Known accuracy boundaries

Fine scanline rendering, sprite evaluation/overflow, composite-video presentation, rare mapper revisions and certain analog/audio behaviors remain under investigation. There are also original ROM fixtures with contradictory or outdated hardware expectations. See [testing](TESTING.md) for the current categorized regression results.

## Portable timeline

`CipherNES_Core/rewind_history.h` is a platform-independent utility for the frontend's bounded rewind timeline; it stores machine snapshots, XOR-differenced frame images, PCM segments and TAS input cursors. State files and movie formats are versioned; do not rely on compatibility across differently built releases without a specific migration test.
