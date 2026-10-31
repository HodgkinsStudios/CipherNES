#include "Mapper064.h"

#include <algorithm>
#include <cstring>

Mapper064::Mapper064(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

uint8_t Mapper064::prgBankCount8k() const {
    return uint8_t(prgBanks * 2);
}

uint16_t Mapper064::chrBankCount1k() const {
    if (chrBanks == 0) return 8;
    return uint16_t(chrBanks * 8);
}

void Mapper064::reset() {
    bankSelect = 0;
    regSelect = 0;
    chrInvert = false;
    prgMode = false;
    chr1kMode = false;
    std::memset(reg, 0, sizeof(reg));

    mirrorMode = 0xFF;

    irqLatch = 0;
    irqCounter = 0;
    irqReload = false;
    irqEnabled = false;
    irqActive = false;
    irqCycleMode = false;
    cpuPrescaler = 0;

    updateBanks();
}

void Mapper064::updateBanks() {
    // --- PRG banks (8KB)
    const uint8_t prgCount = std::max<uint8_t>(1, prgBankCount8k());
    const uint8_t last = uint8_t(prgCount - 1);

    const uint8_t r6 = uint8_t(reg[0x6] % prgCount);
    const uint8_t r7 = uint8_t(reg[0x7] % prgCount);
    const uint8_t rF = uint8_t(reg[0xF] % prgCount);

    if (!prgMode) {
        prgBank[0] = r6;
        prgBank[1] = r7;
        prgBank[2] = rF;
        prgBank[3] = last;
    } else {
        prgBank[0] = rF;
        prgBank[1] = r7;
        prgBank[2] = r6;
        prgBank[3] = last;
    }

    // --- CHR banks (1KB)
    const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());

    // Helper for 2KB mode: R0/R1 are 2KB (even), split into two 1KB slots.
    const uint16_t r0_2k = uint16_t(reg[0x0] & 0xFE);
    const uint16_t r1_2k = uint16_t(reg[0x1] & 0xFE);

    // 1KB banks
    const uint16_t r0_1k = uint16_t(reg[0x0]);
    const uint16_t r1_1k = uint16_t(reg[0x1]);
    const uint16_t r2 = uint16_t(reg[0x2]);
    const uint16_t r3 = uint16_t(reg[0x3]);
    const uint16_t r4 = uint16_t(reg[0x4]);
    const uint16_t r5 = uint16_t(reg[0x5]);
    const uint16_t r8 = uint16_t(reg[0x8]);
    const uint16_t r9 = uint16_t(reg[0x9]);

    if (!chrInvert) {
        if (!chr1kMode) {
            chrBank[0] = (r0_2k + 0) % chrCount;
            chrBank[1] = (r0_2k + 1) % chrCount;
            chrBank[2] = (r1_2k + 0) % chrCount;
            chrBank[3] = (r1_2k + 1) % chrCount;
        } else {
            chrBank[0] = (r0_1k) % chrCount;
            chrBank[1] = (r8) % chrCount;
            chrBank[2] = (r1_1k) % chrCount;
            chrBank[3] = (r9) % chrCount;
        }
        chrBank[4] = (r2) % chrCount;
        chrBank[5] = (r3) % chrCount;
        chrBank[6] = (r4) % chrCount;
        chrBank[7] = (r5) % chrCount;
    } else {
        // Inverted: low 4KB uses R2-R5, high 4KB uses R0/R1(+R8/R9 depending on mode)
        chrBank[0] = (r2) % chrCount;
        chrBank[1] = (r3) % chrCount;
        chrBank[2] = (r4) % chrCount;
        chrBank[3] = (r5) % chrCount;

        if (!chr1kMode) {
            chrBank[4] = (r0_2k + 0) % chrCount;
            chrBank[5] = (r0_2k + 1) % chrCount;
            chrBank[6] = (r1_2k + 0) % chrCount;
            chrBank[7] = (r1_2k + 1) % chrCount;
        } else {
            chrBank[4] = (r0_1k) % chrCount;
            chrBank[5] = (r8) % chrCount;
            chrBank[6] = (r1_1k) % chrCount;
            chrBank[7] = (r9) % chrCount;
        }
    }
}

bool Mapper064::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        mappedAddr = uint32_t(prgBank[0]) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        mappedAddr = uint32_t(prgBank[1]) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        mappedAddr = uint32_t(prgBank[2]) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xE000) {
        mappedAddr = uint32_t(prgBank[3]) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    return false;
}

bool Mapper064::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        if ((addr & 0x0001) == 0) {
            bankSelect = data;
            regSelect = data & 0x0F;
            chrInvert = (data & 0x80) != 0;
            prgMode = (data & 0x40) != 0;
            chr1kMode = (data & 0x20) != 0;
            updateBanks();
        } else {
            // Only known registers are updated; others ignored.
            reg[regSelect] = data;
            updateBanks();
        }
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        if ((addr & 0x0001) == 0) {
            // Mapper 64 mirroring: 0 vertical, 1 horizontal
            mirrorMode = (data & 0x01) ? 0 : 1;
            return true;
        }
        // $A001 unused/unknown
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF) {
        if ((addr & 0x0001) == 0) {
            irqLatch = data;
        } else {
            // Mode select + reload request
            irqCycleMode = (data & 0x01) != 0;
            irqReload = true;
            irqCounter = 0;
            cpuPrescaler = 0;
        }
        return true;
    }

    if (addr >= 0xE000) {
        if ((addr & 0x0001) == 0) {
            irqEnabled = false;
            irqActive = false;
        } else {
            irqEnabled = true;
        }
        return true;
    }

    return false;
}

bool Mapper064::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        const uint8_t bank = uint8_t((addr >> 10) & 0x07);
        mappedAddr = uint32_t(chrBank[bank]) * 0x0400u + uint32_t(addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper064::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            const uint8_t bank = uint8_t((addr >> 10) & 0x07);
            mappedAddr = uint32_t(chrBank[bank]) * 0x0400u + uint32_t(addr & 0x03FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper064::scanline() {
    // Scanline-mode IRQ (MMC3-like approximation)
    if (irqCycleMode) return;

    if (irqCounter == 0 || irqReload) {
        irqCounter = irqLatch;
        irqReload = false;
    } else {
        irqCounter--;
    }

    if (irqCounter == 0 && irqEnabled) {
        irqActive = true;
    }
}

void Mapper064::cpuClock() {
    if (!irqCycleMode) return;
    if (!irqEnabled) return;

    // Clock every 4 CPU cycles
    cpuPrescaler = uint8_t((cpuPrescaler + 1) & 0x03);
    if (cpuPrescaler != 0) return;

    if (irqCounter == 0 || irqReload) {
        irqCounter = irqLatch;
        irqReload = false;
    } else {
        irqCounter--;
    }

    if (irqCounter == 0 && irqEnabled) {
        irqActive = true;
    }
}
