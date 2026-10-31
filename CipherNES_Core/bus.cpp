#include "bus.h"

#include "cartridge.h"
#include "cpu6502.h"

#include "state_io.h"

#ifdef CIPHERNES_DEBUG_DMC_SYNC
#include <cstdio>
#endif

#ifdef CIPHERNES_DEBUG_APU_DMA
#include <cstdio>
#endif
#ifdef CIPHERNES_DEBUG_ABORT_TABLES
#include <cstdio>
#endif

void Bus::rebuildPatchLookup() {
    patchFlags.fill(0);
    patchValue.fill(0);
    patchCompare.fill(0);
    hasPatches = false;

    for (size_t i = 0; i < patches.size(); ++i) {
        const Patch& p = patches[i];
        if (p.addr < 0x8000) continue;
        const uint16_t idx = (uint16_t)(p.addr - 0x8000);
        patchValue[idx] = p.value;
        patchCompare[idx] = p.compare;
        patchFlags[idx] = (uint8_t)(0x01 | (p.hasCompare ? 0x02 : 0x00));
        hasPatches = true;
    }
}

void Bus::clearPatches() {
    patches.clear();
    hasPatches = false;
    patchFlags.fill(0);
    patchValue.fill(0);
    patchCompare.fill(0);
}

void Bus::insertCartridge(const std::shared_ptr<Cartridge>& cart) {
    cartridge = cart;
    cartridgeUsesCpuClock = cart && cart->usesCpuClock();
    cartridgeUsesPpuClock = cart && cart->usesPpuClock();
    cartridgeCanAssertIrq = cart && cart->canAssertIrq();
    ppu.connectCartridge(cart);
    apu.connectBus(this);
}

void Bus::reset(bool hardReset) {
    // In a full NES, we'd also reset APU and other I/O.
    systemClockCounter = 0;
    cpuClockDivider = 0;
    cpuCyclesRemaining = 0;
    nmiPending = false;
    ppuCtrlNmiDelay = 0;
    ppuCtrlNmiWritesThisCpuClock = 0;

    controllerStrobe = false;
    controllerState[0] = 0x00;
    controllerState[1] = 0x00;
    controllerShiftDelay[0] = 0;
    controllerShiftDelay[1] = 0;
    controllerStrobed = false;
    lastControllerRead = false;
    lastControllerReadIndex = 0;

    dmaTransfer = false;
    dmaHalt = false;
    dmaAligned = false;
    dmaFirstCycle = false;
    dmaPage = 0x00;
    dmaAddr = 0x00;
    dmaData = 0x00;
    cpuOpenBus = 0x00;
    cpuInternalBus = 0x00;
    cpuAddressBus = 0x0000;
    cpuBusGetCycle = true;
    dmcDmaTransfer = false;
    dmcDmaHalt = false;
    dmcDmaAbort = false;
    dmcDmaCooldown = 0;
    dmcDmaHaltedLastRead = false;
    dmaPreviousReadAddress = 0x0000;

    // Reset cartridge mapper state too so soft/hard
    // reset behaves like normal mappers instead of leaving coprocessor state stale.
    if (cartridge) {
        cartridge->resetMapper(hardReset);
    }

    ppu.reset();

    if (hardReset) apu.reset();
    else apu.softReset();

    // Ensure the CPU is reset as part of a whole-machine reset so the
    // reset vector ($FFFC/$FFFD) is fetched from the cartridge.
    if (cpu) {
        cpu->reset(hardReset);
        cpuCyclesRemaining = 0;
    }
}

void Bus::advanceCpuBusPhase() {
    apu.clockCpuBusAccess(cpuBusGetCycle);
    cpuBusGetCycle = !cpuBusGetCycle;
}

uint8_t Bus::dmaFetch(uint16_t addr, uint16_t haltedAddress) {
    const uint8_t savedInternalBus = cpuInternalBus;
    const uint16_t savedAddressBus = cpuAddressBus;
    const bool pinsDriven = addr < 0x4000 || addr >= 0x8000;
    uint8_t externalValue = cpuOpenBus;

    if (addr < 0x4000 || addr >= 0x4020) {
        externalValue = cpuRead(addr, false);
    } else {
        advanceCpuBusPhase();
        lastControllerRead = false;
    }

    cpuInternalBus = savedInternalBus;
    cpuAddressBus = savedAddressBus;
    uint8_t value = externalValue;

    if (haltedAddress >= 0x4000 && haltedAddress <= 0x401F) {
        const uint16_t internalAddr = uint16_t(0x4000 | (addr & 0x001F));
        if (internalAddr == 0x4015) {
            value = uint8_t((apu.cpuRead(0x4015, false) & 0xDF) | (externalValue & 0x20));
            cpuOpenBus = externalValue;
            dmaPreviousReadAddress = internalAddr;
            return value;
        } else if (internalAddr == 0x4016 || internalAddr == 0x4017) {
            const uint8_t index = uint8_t(internalAddr & 1);
            const uint8_t controllerValue = uint8_t((value & 0xE0)
                | ((controllerStrobe ? controller[index] : controllerState[index]) & 0x80 ? 1 : 0));

            // The controller CLK line stays low for a continuous run of reads
            // from the same port.  Every DMA-side read therefore postpones the
            // single pending shift; it must not let the two-cycle delay expire
            // in the middle of the DMC halt/dummy/sample sequence.  This also
            // makes a DMC sample-address conflict with $4016/$4017 count as the
            // immediately preceding read for the CPU access that resumes next.
            controllerShiftDelay[index] = 2;
            controllerStrobed = false;

            if (!(dmaTransfer && pinsDriven)) {
                value = controllerValue;
            }
            dmaPreviousReadAddress = internalAddr;
            cpuOpenBus = value;
            return value;
        }
    }

    cpuOpenBus = value;
    dmaPreviousReadAddress = addr;
    return value;
}

void Bus::clockDmaCycle(uint16_t haltedAddress) {
    if (dmaFirstCycle && dmaTransfer) {
        dmaFirstCycle = false;
        if (cpuBusGetCycle) {
            dmaHalt = true;
        }
    }

    if (!cpuBusGetCycle) {
        // PUT cycle: OAM owns the bus. DMC burns a dummy read when OAM is idle.
        if (dmaTransfer && !dmaHalt) {
            if (dmaAligned) {
                advanceCpuBusPhase();
                ppu.dmaWriteOam(dmaData);
                cpuOpenBus = dmaData;
                dmaAddr++;
                if (dmaAddr == 0x00) {
                    dmaTransfer = false;
                    dmaAligned = false;
                }
            } else {
                dmaFetch(haltedAddress, haltedAddress);
            }
        } else if (dmcDmaTransfer || dmaTransfer) {
            dmaFetch(haltedAddress, haltedAddress);
        }
    } else {
        // GET cycle: DMC has priority over OAM.
        if (dmcDmaTransfer && !dmcDmaHalt) {
            const uint8_t sample = dmaFetch(apu.dmcDmaAddress(), haltedAddress);
            apu.completeDmcDma(sample);
            dmcDmaTransfer = false;
            dmcDmaAbort = false;
            dmcDmaCooldown = 2;
            dmaAligned = false;
            dmcDmaHaltedLastRead = true;
        } else if (dmaTransfer && !dmaHalt) {
            dmaData = dmaFetch(uint16_t(uint16_t(dmaPage) << 8) | dmaAddr, haltedAddress);
            dmaAligned = true;
        } else if (dmcDmaTransfer || dmaTransfer) {
            dmaFetch(haltedAddress, haltedAddress);
        }

        dmcDmaHalt = false;
        dmaHalt = false;
    }

    if (dmcDmaAbort) {
        dmcDmaAbort = false;
        if (!apu.hasDmcDmaRequest()) {
            dmcDmaTransfer = false;
            dmcDmaHalt = false;
        }
    }
}

void Bus::clock() {
    // PPU always ticks
    ppu.clock();
    if (cartridgeUsesPpuClock) {
        cartridge->ppuClock(ppu.currentScanline(), ppu.currentCycle(), systemClockCounter);
    }

    // Edge-latch NMI requests from PPU (raised at vblank start when enabled)
    bool nmiLatchedThisMasterClock = false;
    if (ppu.nmi) {
        nmiPending = true;
        nmiLatchedThisMasterClock = true;
        ppu.nmi = false;
    }

    // CPU ticks every 3 PPU cycles.  Use an explicit divider instead of
    // systemClockCounter % 3 because this runs millions of times per second.
    if (cpuClockDivider == 0) {
        // Preserve this cycle's M2 phase for controller strobe processing,
        // which must observe writes made by the bus operation below.
        const bool cycleWasGet = cpuBusGetCycle;
        const bool dmcRequestBeforeApu = apu.hasDmcDmaRequest();

        apu.clock();
        // The verified timing model clocks the DMC output unit after the current CPU gate. If
        // CipherNES raises a reload request in its pre-gate APU step on a GET
        // cycle, leave it pending until the following PUT cycle. Requests
        // already pending before this step (including load DMA) are eligible.
        const bool dmcRequestAfterApu = apu.hasDmcDmaRequest();
        uint16_t haltedAddress = cpu ? cpu->PC : cpuAddressBus;
        bool upcomingCpuWrite = false;
        // The microcycle classifier is purely observational, but it is not
        // free. Ordinary CPU execution does not need the upcoming address or
        // direction at all. Decode it only when DMA arbitration or a mapper's
        // per-M2 callback can consume the result.
        const bool needUpcomingBusAccess = cartridgeUsesCpuClock
            || dmaTransfer || dmcDmaTransfer || dmcRequestAfterApu;
        if (cpu && needUpcomingBusAccess) {
            cpu->upcomingBusAccess(haltedAddress, upcomingCpuWrite);
        }
        const bool deferNewGetPhaseDmcRequest =
            cycleWasGet && !upcomingCpuWrite
            && !dmcRequestBeforeApu && dmcRequestAfterApu;

        // Mappers that use cycle IRQs need to see every CPU cycle (M2), even during DMA stalls.
        if (cartridgeUsesCpuClock) {
            cartridge->cpuClockEx(systemClockCounter, cpuAddressBus, upcomingCpuWrite, dmaTransfer || dmcDmaTransfer || dmaHalt || dmcDmaHalt);
        }

        if (cycleWasGet && dmcDmaCooldown > 0) {
            if (apu.isDmcBoundaryFetchRequest()) {
                apu.rejectDmcBoundaryFetchForCooldown();
            }
            dmcDmaCooldown = uint8_t(dmcDmaCooldown >= 2 ? dmcDmaCooldown - 2 : 0);
        }

        // The verified timing model gates every pending DMC DMA cycle with the delayed $4015
        // enable state. A request copied into the bus arbiter must stop once
        // that delayed disable becomes effective.
        if (dmcDmaTransfer && !apu.isDmcDmaBusActive()
            && !apu.hasDmcDmaRequest()) {
            dmcDmaTransfer = false;
            dmcDmaHalt = false;
            dmcDmaAbort = false;
        }

        if (!dmcDmaTransfer
            && (dmcDmaCooldown == 0 || !apu.isDmcBoundaryFetchRequest())
            && apu.hasDmcDmaRequest() && !deferNewGetPhaseDmcRequest) {
            dmcDmaTransfer = true;
            dmcDmaHalt = true;
            dmcDmaAbort = false;
        }

        // CPU state has not changed since the combined address/direction
        // classification above. Reuse both results for DMA arbitration.
        // Reuse it instead of re-running the full microcycle classifier.
        const bool cpuReadCycle = !cpu || !upcomingCpuWrite;
        const bool dmcBusActive = dmcDmaTransfer && apu.isDmcDmaBusActive();

        bool dmcDmaCycleRan = false;
        if (cpuReadCycle && (dmcBusActive || dmaTransfer)) {
            dmcDmaCycleRan = dmcBusActive;
            if (cpu) cpu->notifyDmaCycle();
            clockDmaCycle(haltedAddress);
        } else if (cpu) {
            // Cycle-accurate CPU ticking: one CPU clock every 3 PPU clocks.
            // Latch NMI requests and let the CPU service them between instructions.
            if (nmiPending && !nmiLatchedThisMasterClock) {
                cpu->nmi();
                nmiPending = false;
            }

            // APU IRQ is level-sensitive
            if (apu.irq) {
                cpu->irq();
            }
            // Cartridge IRQ (e.g., MMC3) is also level-sensitive
            if (cartridgeCanAssertIrq && cartridge->irqState()) {
                cpu->irq();
            }
            ppuCtrlNmiWritesThisCpuClock = 0;
            cpu->clock();
            if (ppuCtrlNmiDelay > 0) {
                ppuCtrlNmiDelay--;
                if (ppuCtrlNmiDelay == 0) {
                    nmiPending = true;
                }
            }
        }

        if (apu.finishDmcImplicitAbortCycle(dmcDmaCycleRan)) {
            dmcDmaTransfer = false;
            dmcDmaHalt = false;
            dmcDmaAbort = false;
        }

        apu.clockFrameIrqPulse();
        apu.clockDmcOutput();

        // Initial DMC sample loading starts after two APU PUT phases.
        apu.clockDmcLoadDma(!cycleWasGet);

        // Controller strobing observes writes from this bus cycle.
        if (controllerStrobe) {
            controllerShiftDelay[0] = 0;
            controllerShiftDelay[1] = 0;
            if (cycleWasGet && !controllerStrobed) {
                controllerStrobed = true;
                controllerState[0] = controller[0];
                controllerState[1] = controller[1];
            }
        } else {
            controllerStrobed = false;
            for (uint8_t i = 0; i < 2; i++) {
                if (controllerShiftDelay[i] > 0) {
                    controllerShiftDelay[i]--;
                    if (controllerShiftDelay[i] == 0) {
                        controllerState[i] <<= 1;
                        controllerState[i] |= 0x01;
                    }
                }
            }
        }

    }

    cpuClockDivider++;
    if (cpuClockDivider >= 3) cpuClockDivider = 0;

    systemClockCounter++;
}

uint8_t Bus::cpuRead(uint16_t addr, bool readOnly) {
    if (!readOnly) {
        cpuAddressBus = addr;
        dmcDmaHaltedLastRead = false;
        advanceCpuBusPhase();
    }

    uint8_t data = 0x00;
    auto finishRead = [&](uint8_t v, bool controllerRead = false, uint8_t controllerIndex = 0,
                          bool updateInternalBus = true) -> uint8_t {
        if (!readOnly) {
            lastControllerRead = controllerRead;
            lastControllerReadIndex = controllerIndex;
            if (updateInternalBus) cpuInternalBus = v;
        }
        return v;
    };

    // Cartridge address space (Mapper decides if it handles this address)
    if (cartridge && cartridge->cpuRead(addr, data)) {
        // Apply patches on PRG-ROM space reads
        if (hasPatches && addr >= 0x8000) {
            const uint16_t idx = (uint16_t)(addr - 0x8000);
            const uint8_t flags = patchFlags[idx];
            if (flags & 0x01) {
                if (!(flags & 0x02) || patchCompare[idx] == data) {
                    data = patchValue[idx];
                    if (!readOnly) cpuOpenBus = data;
                    return finishRead(data);
                }
            }
        }
        if (!readOnly) cpuOpenBus = data;
        return finishRead(data);
    }

    // 2KB internal RAM, mirrored across $0000-$1FFF
    if (addr <= 0x1FFF) {
        data = ram[addr & 0x07FF];
        if (!readOnly) cpuOpenBus = data;
        return finishRead(data);
    }

    // PPU registers $2000-$2007 mirrored up to $3FFF
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        const uint16_t ppuRegister = addr & 0x2007;
        data = ppu.cpuRead(ppuRegister, readOnly);
        if (!readOnly) {
            cpuOpenBus = data;
            // Reads at PPU dots 2-4 are the documented vblank/NMI suppression
            // window. Dot 4 can cancel an edge already sampled for the CPU's
            // next instruction, but not one whose interrupt entry has begun.
            if (ppuRegister == 0x2002
                && ppu.currentScanline() == 241
                && ppu.currentCycle() >= 2 && ppu.currentCycle() <= 4) {
                nmiPending = false;
                ppuCtrlNmiDelay = 0;
                if (cpu) cpu->cancelPendingNmi();
            }
        }
        return finishRead(data);
    }

    // APU and I/O
    if (addr >= 0x4000 && addr <= 0x4017) {
        // APU registers + a few I/O registers are in this range.
        // Let APU handle the registers it owns. Controllers are handled below.
        if (addr == 0x4015) {
            data = uint8_t((apu.cpuRead(addr, readOnly) & 0xDF) | (cpuInternalBus & 0x20));
            return finishRead(data, false, 0, false);
        }
        if (addr != 0x4016 && addr != 0x4017) {
#ifdef CIPHERNES_DEBUG_DMC_SYNC
            static int apuOpenReadLogCount = 0;
            if (!readOnly && addr == 0x4000 && ram[0x0010] >= 0x12 && apuOpenReadLogCount < 400) {
                std::fprintf(stderr, "READ4000 val=%02X delay=%u transfer=%d req=%d err=%02X pc=%04X cyc=%llu get=%d\n",
                             cpuOpenBus, dmcDmaHalt ? 1u : 0u, dmcDmaTransfer ? 1 : 0,
                             apu.hasDmcDmaRequest() ? 1 : 0, ram[0x0010], cpu ? cpu->PC : 0,
                             (unsigned long long)systemClockCounter, cpuBusGetCycle ? 1 : 0);
                apuOpenReadLogCount++;
            }
#endif
            return finishRead(cpuOpenBus);
        }
    }

    if (addr == 0x4016 || addr == 0x4017) {
        // NES controller reads:
        //  - While strobe is high, reads return the current state of the A button (no shifting)
        //  - While strobe is low, reads shift out latched button states in the order A,B,Select,Start,Up,Down,Left,Right
        const uint8_t idx = uint8_t(addr & 0x0001);

        if (controllerStrobe) {
            // Continuously reflect current A button (bit 7 in our MSB-first packing)
            data = uint8_t((cpuOpenBus & 0xE0) | ((controller[idx] & 0x80) ? 1 : 0));
            if (!readOnly) cpuOpenBus = data;
            return finishRead(data, true, idx);
        }

        data = uint8_t((cpuOpenBus & 0xE0) | ((controllerState[idx] & 0x80) ? 1 : 0));
        if (!readOnly) {
            controllerShiftDelay[idx] = 2;
        }
        if (!readOnly) cpuOpenBus = data;
        return finishRead(data, true, idx);
    }

    return finishRead(cpuOpenBus);
}

void Bus::cpuWrite(uint16_t addr, uint8_t data) {
    if (apu.cancelDmcImplicitAbortOnWrite(dmcDmaHalt)) {
        dmcDmaTransfer = false;
        dmcDmaHalt = false;
        dmcDmaAbort = false;
    }
    cpuAddressBus = addr;
    apu.clockCpuBusAccess(cpuBusGetCycle);
    cpuBusGetCycle = !cpuBusGetCycle;
    cpuOpenBus = data;
    cpuInternalBus = data;
    lastControllerRead = false;
    // Cartridge address space
    if (cartridge && cartridge->cpuWrite(addr, data)) {
        dmcDmaHaltedLastRead = false;
        return;
    }

    // 2KB internal RAM, mirrored across $0000-$1FFF
    if (addr <= 0x1FFF) {
        uint16_t a = addr & 0x07FF;
        ram[a] = data;
#ifdef CIPHERNES_DEBUG_ABORT_TABLES
        if ((a == 0x0478 || a == 0x0479) && data != 0) {
            std::fprintf(stderr, "RESULT %04X=%02X\n", a, data);
            for (uint16_t base : {uint16_t(0x0050), uint16_t(0x0500), uint16_t(0x0520), uint16_t(0x0540)}) {
                std::fprintf(stderr, "%04X:", base);
                for (uint16_t i = 0; i < 16; i++) {
                    std::fprintf(stderr, " %02X", ram[base + i]);
                }
                std::fprintf(stderr, "\n");
            }
        }
#endif
        dmcDmaHaltedLastRead = false;
        return;
    }

    // PPU registers $2000-$2007 mirrored up to $3FFF
    if (addr >= 0x2000 && addr <= 0x3FFF) {
        const bool ppuCtrlWrite = (addr & 0x2007) == 0x2000;
        const uint8_t ppuCtrlWriteIndex = ppuCtrlNmiWritesThisCpuClock;
        if (ppuCtrlWrite) {
            ppuCtrlNmiWritesThisCpuClock++;
        }
        ppu.cpuWrite(addr & 0x2007, data);
        if (ppuCtrlWrite && (data & 0x80) == 0
            && ppu.currentScanline() == 241
            && ppu.currentCycle() >= 2 && ppu.currentCycle() <= 4) {
            nmiPending = false;
            ppuCtrlNmiDelay = 0;
            if (cpu) cpu->cancelPendingNmi();
        }
        if (ppuCtrlWrite && ppu.nmi) {
            ppu.nmi = false;
            if (ppuCtrlWriteIndex == 0) {
                ppuCtrlNmiDelay = 1;
            } else {
                ppuCtrlNmiDelay = 1;
            }
        }
        dmcDmaHaltedLastRead = false;
        return;
    }

    // OAMDMA ($4014)
    if (addr == 0x4014) {
        // Initiate DMA transfer from CPU page "data" into PPU OAM.
        dmaPage = data;
        dmaAddr = 0x00;
        dmaTransfer = true;
        dmaHalt = false;
        dmaAligned = false;
        dmaFirstCycle = true;
        dmcDmaHaltedLastRead = false;
        return;
    }

    // Controller strobe ($4016)
    if (addr == 0x4016) {
        controllerStrobe = (data & 0x01) != 0;
        if (!controllerStrobe) controllerStrobed = false;
        dmcDmaHaltedLastRead = false;
        return;
    }

    // APU registers
    if (addr >= 0x4000 && addr <= 0x4017) {
        // Note: $4014 (DMA) and $4016 (controller) already handled above.
        if (addr != 0x4014 && addr != 0x4016) {
#ifdef CIPHERNES_DEBUG_ABORT_TABLES
            if (addr == 0x4015 && cpu) {
                std::fprintf(stderr,
                             "BUS_%s x=%02X transfer=%d halt=%d get=%d cooldown=%u\n",
                             (data & 0x10) ? "ENABLE" : "DISABLE",
                             cpu->X, dmcDmaTransfer ? 1 : 0, dmcDmaHalt ? 1 : 0,
                             cpuBusGetCycle ? 1 : 0, dmcDmaCooldown);
            }
#endif
            apu.cpuWrite(addr, data);
        }
        dmcDmaHaltedLastRead = false;
        return;
    }
    dmcDmaHaltedLastRead = false;
}

// ------------------------------
// Save-state support
// ------------------------------
void Bus::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_bytes(out, ram.data(), ram.size());

    write_u8(out, controller[0]);
    write_u8(out, controller[1]);
    write_u8(out, controllerState[0]);
    write_u8(out, controllerState[1]);
    write_u8(out, controllerShiftDelay[0]);
    write_u8(out, controllerShiftDelay[1]);
    write_bool(out, controllerStrobe);

    write_u64(out, systemClockCounter);
    write_pod(out, cpuCyclesRemaining);
    write_bool(out, nmiPending);

    write_bool(out, dmaTransfer);
    write_bool(out, dmaHalt);
    write_u8(out, dmaPage);
    write_u8(out, dmaAddr);
    write_u8(out, dmaData);
    write_bool(out, dmcDmaHaltedLastRead);

    // PPU/APU/CPU
    ppu.saveState(out);
    apu.saveState(out);
    if (cpu) {
        cpu->saveState(out);
    } else {
        // placeholder
        write_u8(out, 0);
    }

    // Cartridge
    if (cartridge) {
        write_bool(out, true);
        cartridge->saveState(out);
    } else {
        write_bool(out, false);
    }

    // Patches
    write_u32(out, (uint32_t)patches.size());
    for (const auto& pa : patches) {
        write_u16(out, pa.addr);
        write_u8(out, pa.value);
        write_bool(out, pa.hasCompare);
        write_u8(out, pa.compare);
    }

    // State v6 compatibility: obsolete RAM-lock count (always zero).
    write_u32(out, 0u);
}

bool Bus::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_bytes(p,end,ram.data(),ram.size())) return false;

    if (!read_u8(p,end,controller[0])) return false;
    if (!read_u8(p,end,controller[1])) return false;
    if (!read_u8(p,end,controllerState[0])) return false;
    if (!read_u8(p,end,controllerState[1])) return false;
    if (p < end) {
        if (!read_u8(p,end,controllerShiftDelay[0])) return false;
        if (!read_u8(p,end,controllerShiftDelay[1])) return false;
    } else {
        controllerShiftDelay[0] = 0;
        controllerShiftDelay[1] = 0;
    }
    if (!read_bool(p,end,controllerStrobe)) return false;

    if (!read_u64(p,end,systemClockCounter)) return false;
    cpuClockDivider = uint8_t(systemClockCounter % 3ULL);
    if (!read_pod(p,end,cpuCyclesRemaining)) return false;
    if (!read_bool(p,end,nmiPending)) return false;
    ppuCtrlNmiDelay = 0;
    ppuCtrlNmiWritesThisCpuClock = 0;

    if (!read_bool(p,end,dmaTransfer)) return false;
    if (!read_bool(p,end,dmaHalt)) return false;
    if (!read_u8(p,end,dmaPage)) return false;
    if (!read_u8(p,end,dmaAddr)) return false;
    if (!read_u8(p,end,dmaData)) return false;
    if (p < end) {
        if (!read_bool(p,end,dmcDmaHaltedLastRead)) return false;
    } else {
        dmcDmaHaltedLastRead = false;
    }
    dmaAligned = false;
    dmaFirstCycle = dmaTransfer;
    dmcDmaTransfer = false;
    dmcDmaHalt = false;
    dmcDmaAbort = false;
    dmcDmaCooldown = 0;
    dmaPreviousReadAddress = cpuAddressBus;

    if (!ppu.loadState(p,end)) return false;
    if (!apu.loadState(p,end)) return false;
    if (cpu) {
        if (!cpu->loadState(p,end)) return false;
    } else {
        uint8_t dummy=0; if(!read_u8(p,end,dummy)) return false;
    }

    bool hasCart=false; if(!read_bool(p,end,hasCart)) return false;
    if (hasCart) {
        if (!cartridge) return false;
        if (!cartridge->loadState(p,end)) return false;
    }

    uint32_t patchCount=0; if(!read_u32(p,end,patchCount)) return false;
    patches.clear();
    patches.reserve(patchCount);
    for (uint32_t i=0;i<patchCount;i++) {
        Patch pa;
        if(!read_u16(p,end,pa.addr)) return false;
        if(!read_u8(p,end,pa.value)) return false;
        if(!read_bool(p,end,pa.hasCompare)) return false;
        if(!read_u8(p,end,pa.compare)) return false;
        patches.push_back(pa);
    }
    rebuildPatchLookup();

    uint32_t lockCount=0; if(!read_u32(p,end,lockCount)) return false;
    // Read and discard historical RAM locks; they are not exposed by the 1.0 UI.
    if (lockCount > 2048u) return false;
    for (uint32_t i=0;i<lockCount;i++) {
        uint16_t ignoredAddr=0; uint8_t ignoredValue=0;
        if(!read_u16(p,end,ignoredAddr) || !read_u8(p,end,ignoredValue)) return false;
    }

    return true;
}
