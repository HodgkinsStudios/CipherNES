#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "ppu2C02.h"
#include "apu2A03.h"

class Cartridge;
struct CPU6502;

// NES CPU bus (early stage)
//
// Implements:
//  - 2KB internal RAM mirrored across $0000-$1FFF
//  - Cartridge mapping for CPU address space (Mapper 0)
//  - PPU register mapping ($2000-$2007 mirrored)
//  - Controller strobe + shift ($4016/$4017)
//  - OAM DMA ($4014)
//  - Master clock (PPU every tick, CPU every 3 ticks) + vblank NMI wiring
struct Bus {
    std::array<uint8_t, 2048> ram{}; // $0000-$07FF

    std::shared_ptr<Cartridge> cartridge;
    // Immutable mapper feature flags cached at cartridge insertion. These avoid
    // virtual feature-query calls on every CPU/PPU master cycle.
    bool cartridgeUsesCpuClock = false;
    bool cartridgeUsesPpuClock = false;
    bool cartridgeCanAssertIrq = false;

    // PPU (register-level + simple software frame renderer)
    PPU2C02 ppu;

    // APU (audio)
    APU2A03 apu;

    // CPU is owned elsewhere but bus drives it via clock().
    CPU6502* cpu = nullptr;

    // Controller state (latched on write to $4016)
    std::array<uint8_t, 2> controller{};
    std::array<uint8_t, 2> controllerState{};
    std::array<uint8_t, 2> controllerShiftDelay{};
    bool controllerStrobe = false;
    bool controllerStrobed = false;
    bool lastControllerRead = false;
    uint8_t lastControllerReadIndex = 0;

    // Master clock state
    uint64_t systemClockCounter = 0; // one tick = one PPU cycle
    // Faster CPU/PPU divider than systemClockCounter % 3 on every master tick.
    // Kept derived from systemClockCounter for save-state compatibility.
    uint8_t cpuClockDivider = 0;
    int cpuCyclesRemaining = 0;
    bool nmiPending = false; // edge-latched NMI pending for CPU
    uint8_t ppuCtrlNmiDelay = 0;
    uint8_t ppuCtrlNmiWritesThisCpuClock = 0;

    // OAM DMA
    bool dmaTransfer = false;
    bool dmaHalt = false;
    bool dmaAligned = false;
    bool dmaFirstCycle = false;
    uint8_t dmaPage = 0x00;
    uint8_t dmaAddr = 0x00;
    uint8_t dmaData = 0x00;
    uint8_t cpuOpenBus = 0x00;
    uint8_t cpuInternalBus = 0x00;
    uint16_t cpuAddressBus = 0x0000;
    bool cpuBusGetCycle = true;

    // DMC sample DMA
    bool dmcDmaTransfer = false;
    bool dmcDmaHalt = false;
    bool dmcDmaAbort = false;
    uint8_t dmcDmaCooldown = 0;
    bool dmcDmaHaltedLastRead = false;
    uint16_t dmaPreviousReadAddress = 0x0000;

    // Cheats: Game Genie style patches (applied on CPU reads in $8000-$FFFF)
    struct Patch {
        uint16_t addr = 0;
        uint8_t  value = 0;
        bool     hasCompare = false;
        uint8_t  compare = 0;
    };
    std::vector<Patch> patches;
    bool hasPatches = false;
    // Fast O(1) Game Genie lookup for PRG-ROM reads.  patchFlags bit 0 = present, bit 1 = compare enabled.
    std::array<uint8_t, 0x8000> patchValue{};
    std::array<uint8_t, 0x8000> patchCompare{};
    std::array<uint8_t, 0x8000> patchFlags{};


    void insertCartridge(const std::shared_ptr<Cartridge>& cart);
    void reset(bool hardReset = true);
    void rebuildPatchLookup();
    void clearPatches();

    // Save-state support
    void saveState(std::vector<uint8_t>& out) const;
    bool loadState(const uint8_t*& p, const uint8_t* end);

    // Run one master clock tick (PPU ticks every call; CPU ticks every 3 calls)
    void clock();
    void clockDmaCycle(uint16_t haltedAddress);
    uint8_t dmaFetch(uint16_t addr, uint16_t haltedAddress);
    void advanceCpuBusPhase();

    // readOnly is useful later for disassembly/debug
    uint8_t cpuRead(uint16_t addr, bool readOnly = false);
    void cpuWrite(uint16_t addr, uint8_t data);
};
