#include "../state_io.h"
#include "Mapper064.h"

#include <algorithm>
#include <cstring>

Mapper064::Mapper064(uint8_t prgBanks, uint8_t chrBanks, bool variant) : Mapper(prgBanks, chrBanks), variant158(variant) {
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
    ciram.fill(0);
    std::memset(ntPage, 0, sizeof(ntPage));

    irqLatch = 0;
    irqCounter = 0;
    irqReload = false;
    irqEnabled = false;
    irqActive = false;
    irqCycleMode = false;
    cpuPrescaler = 0;
    irqDelay = 0;
    a12Previous = false;
    a12LowDots = 0;

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
    if (variant158) {
        // 800037: bit 7 of the selected *raw* CHR bank controls CIRAM
        // A10.  Do not inspect chrBank[] after modulo, because ROMs with
        // 128 KiB CHR memory would discard that bit.
        if (!chrInvert) {
            if (!chr1kMode) {
                ntPage[0] = uint8_t(reg[0] >> 7);
                ntPage[1] = uint8_t(reg[0] >> 7);
                ntPage[2] = uint8_t(reg[1] >> 7);
                ntPage[3] = uint8_t(reg[1] >> 7);
            } else {
                ntPage[0] = uint8_t(reg[0] >> 7);
                ntPage[1] = uint8_t(reg[8] >> 7);
                ntPage[2] = uint8_t(reg[1] >> 7);
                ntPage[3] = uint8_t(reg[9] >> 7);
            }
        } else {
            for (unsigned i=0; i<4; ++i) ntPage[i] = uint8_t(reg[i+2] >> 7);
        }
    }
}

bool Mapper064::nametableRead(uint16_t addr, uint8_t& data) {
    if (!variant158 || (addr & 0x3FFFu) < 0x2000u || (addr & 0x3FFFu) >= 0x3F00u)
        return false;
    const uint16_t offset = uint16_t((addr - 0x2000u) & 0x0FFFu);
    data = ciram[uint16_t(ntPage[offset >> 10]) * 0x400u + (offset & 0x3FFu)];
    return true;
}

bool Mapper064::nametableWrite(uint16_t addr, uint8_t data) {
    if (!variant158 || (addr & 0x3FFFu) < 0x2000u || (addr & 0x3FFFu) >= 0x3F00u)
        return false;
    const uint16_t offset = uint16_t((addr - 0x2000u) & 0x0FFFu);
    ciram[uint16_t(ntPage[offset >> 10]) * 0x400u + (offset & 0x3FFu)] = data;
    return true;
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
            if (!variant158) mirrorMode = (data & 0x01) ? 0 : 1;
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
            irqDelay = 0;
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

void Mapper064::clockIrqCounter() {
    // RAMBO-1 differs from MMC3: an explicit $C001 reload gives an odd
    // latch value (nonzero latches OR 1), followed by conventional 0/reload
    // or decrement clocks. The IRQ is delayed on the CPU/M2 domain.
    if (irqReload) {
        irqCounter = uint8_t(irqLatch | (irqLatch ? 1u : 0u));
        irqReload = false;
    } else if (irqCounter == 0) {
        irqCounter = irqLatch;
    } else {
        --irqCounter;
    }
    if (irqCounter == 0 && irqEnabled && !irqActive && !irqDelay) {
        // RAMBO-1 /IRQ is delayed to the M2 domain: two falling edges
        // after PPU A12 in scanline mode, one after the CPU-prescaled
        // counter event in cycle mode.  These are not interchangeable.
        irqDelay = irqCycleMode ? 1u : 2u;
    }
}

void Mapper064::ppuAddressBus(uint16_t addr) {
    // Sample the real PPU bus each dot, not a synthetic once-per-scanline
    // callback. Ignore closely-spaced rises when A12 has not been held low
    // for approximately three CPU cycles (8 PPU dots).
    const bool high = (addr & 0x1000u) != 0;
    if (!high) {
        if (a12LowDots != 255u) ++a12LowDots;
    } else {
        if (!a12Previous && a12LowDots >= 8u && !irqCycleMode)
            clockIrqCounter();
        a12LowDots = 0;
    }
    a12Previous = high;
}

void Mapper064::scanline() {
    // Do not double-clock the counter: PPU address sampling owns scanline IRQs.
}

void Mapper064::cpuClock() {
    if (irqDelay) {
        --irqDelay;
        if (!irqDelay && irqEnabled) irqActive = true;
    }
    if (!irqCycleMode) return;
    if (++cpuPrescaler < 4u) return;
    cpuPrescaler = 0;
    clockIrqCounter();
}

// Exact mutable registers and IRQ state; constant ROM geometry is not serialized.
void Mapper064::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, bankSelect);
    write_u8(out, regSelect);
    write_bool(out, chrInvert);
    write_bool(out, prgMode);
    write_bool(out, chr1kMode);
    for (auto value : reg) write_u8(out, value);
    write_u8(out, mirrorMode);
    write_u8(out, irqLatch);
    write_u8(out, irqCounter);
    write_bool(out, irqReload);
    write_bool(out, irqEnabled);
    write_bool(out, irqActive);
    write_bool(out, irqCycleMode);
    write_u8(out, cpuPrescaler);
    write_u8(out, irqDelay);
    write_bool(out, a12Previous);
    write_u8(out, a12LowDots);
    if (variant158) write_bytes(out, ciram.data(), ciram.size());
}
bool Mapper064::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, bankSelect)) return false;
    if (!read_u8(p, end, regSelect)) return false;
    if (!read_bool(p, end, chrInvert)) return false;
    if (!read_bool(p, end, prgMode)) return false;
    if (!read_bool(p, end, chr1kMode)) return false;
    for (auto& value : reg) if (!read_u8(p, end, value)) return false;
    if (!read_u8(p, end, mirrorMode)) return false;
    if (!read_u8(p, end, irqLatch)) return false;
    if (!read_u8(p, end, irqCounter)) return false;
    if (!read_bool(p, end, irqReload)) return false;
    if (!read_bool(p, end, irqEnabled)) return false;
    if (!read_bool(p, end, irqActive)) return false;
    if (!read_bool(p, end, irqCycleMode)) return false;
    if (!read_u8(p, end, cpuPrescaler)) return false;
    if (!read_u8(p, end, irqDelay)) return false;
    if (!read_bool(p, end, a12Previous)) return false;
    if (!read_u8(p, end, a12LowDots)) return false;
    if (cpuPrescaler >= 4 || irqDelay > 2) return false;
    if (variant158 && !read_bytes(p, end, ciram.data(), ciram.size())) return false;
    updateBanks();
    return true;
}
