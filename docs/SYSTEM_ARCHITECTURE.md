# CipherNES System Architecture

> **Architecture note:** Host-platform integration and the public core ABI are documented separately in [Core API](CORE_LIBRARY.md). This reference describes the current v1.0 source architecture.


CipherNES uses one emulation core for its Windows, macOS, and Linux frontends.

```text
+------------------------------+
| CipherNES Win / macOS / Linux |
|  SDL2 video/audio/input/UI   |
+--------------+---------------+
               |
               | C ABI
               v
+------------------------------+
| Core .dll / .dylib / .so     |
|                              |
|  2A03 CPU   Bus / DMA        |
|  2C02 PPU   2A03 APU         |
|  Cartridge  Built-in mappers |
|  FDS                         |
+------------------------------+
```

The frontend owns presentation and user interaction. The core owns machine state and hardware emulation.

## Repository layout

```text
CipherNES_Core/        Emulation core and built-in mappers
CipherNES_Ubuntu/      Shared Unix frontend / Ubuntu target
CipherNES_Windows/     Native Windows frontend
CipherNES_macOS/       Native macOS frontend target
CipherNES_Ubuntu.cbp   Ubuntu Code::Blocks project
CipherNES_Windows.cbp  Windows Code::Blocks project
build_ubuntu.sh       Ubuntu build script
build_windows.ps1     Windows build script
build_macos.sh        macOS app-bundle build script
run_ciphernes.sh       Ubuntu convenience launcher
docs/                 Public technical documentation
bios/                 External FDS BIOS instructions
```

## Core/frontend boundary

The core is compiled directly into the native executables on Ubuntu, Windows, and macOS. The public C ABI remains defined by `CipherNES_Core/ciphernes_core.h` and remains usable for future shared-library builds when required.

The native desktop frontend handles:

- SDL2 window creation and rendering;
- host audio playback;
- keyboard and controller input;
- menus and configuration;
- OS-native ROM/ZIP and TAS selection plus an OS-native AVI output-folder dialog;
- compressed bounded rewind history, reverse PCM playback and TAS movie branching;
- screenshots and recording;
- TAS frontend tools;
- Game Genie UI;
- save/load user interaction.

The core handles:

- CPU execution;
- PPU timing/rendering;
- APU timing and sample generation;
- controller serial behavior;
- CPU/PPU bus behavior;
- OAM and DMC DMA;
- cartridges and built-in mapper hardware;
- FDS hardware-facing behavior;
- deterministic machine state;
- save-state serialization.

Because the core is separate, every desktop frontend and external core client runs the same emulation code.

## Timing model

CipherNES uses an NTSC timing model centered on the PPU/master clock. The CPU advances at the NES CPU-to-PPU relationship while the PPU and mapper logic observe the corresponding timing progression.

Timing-sensitive behavior is modeled in the core rather than inferred from host frame rate. This includes:

- VBL/NMI timing;
- sprite-zero hit;
- sprite overflow/evaluation;
- `$2004`/OAM behavior;
- OAM DMA;
- DMC DMA and controller-read interactions;
- mapper IRQ counters;
- scanline/fetch-sensitive mapper behavior;
- instruction/interrupt edge cases.

The host can step a master clock, a complete frame, or batches of frames through the public API.

## CPU: Ricoh 2A03 / 6502-family core

The CPU subsystem models the NES 2A03 instruction set and cycle behavior needed by commercial software and accuracy diagnostics.

Responsibilities include opcode execution, addressing modes, page crossings, stack behavior, IRQ/NMI/reset sequencing, read/modify/write bus behavior, dummy reads/writes, instruction timing, unofficial behavior required by NES software, and interaction with DMA stalls.

CPU memory accesses are routed through the bus so cartridge hardware, PPU/APU registers, RAM mirrors, controllers, DMA, and mapper logic observe the appropriate transactions.

## Bus and DMA

The bus integrates CPU execution with NES devices. It handles internal RAM and mirrors, PPU registers, APU/I/O registers, controllers, cartridge memory, mapper register writes, OAM DMA, DMC DMA, contention/ordering, and modeled open-bus behavior.

DMA is not treated as a bulk memory copy; timing interactions with CPU access, controllers, APU/DMC activity, and OAM transfer behavior remain part of the cycle-oriented model.

## PPU: Ricoh 2C02

The PPU models scanline/dot progression, background and sprite fetches, scrolling/address registers, palette RAM, OAM/secondary OAM, sprite evaluation, sprite-zero hit, overflow behavior, VBL/NMI behavior, register semantics, read buffering/open bus, mapper-visible address activity, and final 256x240 ARGB32 output.

AccuracyCoin-related accuracy work included advanced secondary-OAM address/freeze behavior during sprite evaluation and fetch. The updated 146-test suite passed on the repaired Ubuntu core.

## APU: Ricoh 2A03 audio

The APU models Pulse 1, Pulse 2, Triangle, Noise, and DMC channels. Frame-counter timing, envelopes, sweeps, length counters, DMC fetching, IRQ behavior, and host sample generation are handled in the core.

DMC operation is integrated with CPU/bus timing because sample fetches can affect CPU execution, DMA behavior, and controller reads.

## Cartridge loading

The core accepts iNES/NES 2.0 cartridge images plus FDS disk images through file and in-memory paths.

The cartridge layer handles:

- header parsing;
- PRG-ROM and CHR-ROM/CHR-RAM storage;
- PRG-RAM;
- mapper/submapper selection;
- mirroring;
- battery-backed state.

Desktop frontends additionally accept `.zip` files and locate supported NES/FDS images inside them before loading.

## Mapper architecture

Built-in mapper implementations live under `CipherNES_Core/Mappers/`.

Mapper responsibilities may include:

- PRG banking;
- CHR banking;
- PRG-RAM control;
- mirroring;
- IRQ counters;
- CPU/PPU address observation;
- scanline/fetch timing;
- expansion audio;
- mapper-owned persistent or volatile state.

## MMC3/MMC6 hardware profiles

Some mapper-4 tests deliberately distinguish IRQ-counter behavior found in different silicon revisions.

CipherNES can select the alternate revision model rather than forcing every mapper-4 ROM through one behavior. The release validation therefore documents profile-specific MMC3/MMC6 cases separately.

## Famicom Disk System

CipherNES includes FDS emulation while deliberately excluding the copyrighted BIOS from the repository.

The user supplies a legally obtained 8192-byte `disksys.rom`. The core exposes disk-side insertion/ejection controls while the frontend provides the user-facing workflow.

## Controller/input architecture

The core receives two controller bytes and models the NES controller serial interface. The desktop frontend maps keyboard/game-controller state into those bytes.

## Video path

The core renders a 256x240 ARGB32 framebuffer. The frontend retrieves and presents it with SDL2.

This keeps video presentation separate from emulation timing. Headless framebuffer suppression does not disable PPU execution or mapper-visible fetch behavior.

## Audio path

The APU generates samples inside the core. The frontend chooses a host sample rate, drains the core audio buffer, and sends samples to the host audio device.

## 1.0 timeline and movie model

The shared `CipherNES_Core/rewind_history.h` is a header-only, platform-independent frontend utility. It stores a transient machine-state snapshot, losslessly XOR-differenced rendered pixels, a frame's signed 16-bit PCM samples, and the associated TAS input cursor. A zero-run/literal compressor and 256 MB ceiling constrain storage; a 30-second maximum also applies. Reversing a frame decodes the predecessor state and XOR pixel delta; a new branch truncates the discarded future.

The public C ABI `ciphernes_save_state_transient` exposes the existing core state format without invoking battery-RAM disk flushing every frame. Ordinary manual-state operations retain their own persisted state semantics. The core-state version and accuracy-critical execution path are not changed by the history manager.

Desktop `.nss` save slots now contain versioned `CNSSM02!` metadata: ROM identity, compressed displayed ARGB frame, optional exact two-port TAS input prefix. This is outside the core state blob. A legacy state lacking a complete TAS prefix can be loaded outside TAS recording, but not during recording. `.ci` movie export remains compatible with existing `.ci` recordings.

Native file dialogs: Windows uses Win32 common dialogs and Shell folder chooser, macOS uses a Cocoa dialog invoked by AppleScript, Linux uses Zenity (GTK) or KDialog (KDE). These are host-only and introduce no emulation-core dependencies.

## Save states

Save states snapshot the machine state required to resume execution, including built-in mapper state where applicable.

The public API treats state data as an opaque byte sequence. Callers query the required size, save into a caller-provided buffer, and later request a validated load.

## Accuracy certification

The accuracy results were produced with the same emulation code exposed by the public core library. There is no separate test-only emulator core.

Certification coverage includes AccuracyCoin, CPU behavior/timing, APU behavior, DMA/DPCM/OAM, MMC1/MMC3/MMC5 cases, PPU timing and sprite behavior, and deterministic framebuffer comparisons.

See `VALIDATION.md` and `NTSC_COMPATIBILITY.md` for the recorded release results and scope.

## Core/frontend rule

> Hardware emulation belongs in the reusable core; operating-system presentation and user interaction belong in the frontend.

That boundary keeps Windows, macOS, Ubuntu, and external core-library clients on the same machine implementation.
