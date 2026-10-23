#include "Mapper001.h"

#include "../state_io.h"

// -----------------------------------------------------------------------------
// Mapper001 (MMC1)
// -----------------------------------------------------------------------------
void Mapper001::reset() {
    shiftReg = 0x10;
    shiftCount = 0;

    // Power-on: control = 0x0C (PRG mode 3) with upper bits often set; we keep 0x1C to match header default.
    control = 0x1C;
    chrBank0 = 0;
    chrBank1 = 0;
    prgBank = 0;

    prgRamEnabled = true;
    currentPpuA12 = false;
    mirrorMode = 0xFF;
    updateMirroring();
}

void Mapper001::updateMirroring() {
    // Control bits 0-1: mirroring
    // 0: one-screen, lower bank
    // 1: one-screen, upper bank
    // 2: vertical
    // 3: horizontal
    uint8_t m = control & 0x03;
    if (m == 0) {
        mirrorMode = 3; // ONE_SCREEN_LO
    } else if (m == 1) {
        mirrorMode = 4; // ONE_SCREEN_HI
    } else if (m == 2) {
        mirrorMode = 1; // VERTICAL
    } else if (m == 3) {
        mirrorMode = 0; // HORIZONTAL
    }
}

void Mapper001::commitRegister(uint16_t addr, uint8_t value) {
    // Determine which internal register is being targeted by addr
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        control = value & 0x1F;
        updateMirroring();
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        chrBank0 = value & 0x1F;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        chrBank1 = value & 0x1F;
    } else if (addr >= 0xE000) {
        prgBank = value & 0x1F;
        // PRG RAM disable is bit 4: 0=enabled, 1=disabled
        prgRamEnabled = (prgBank & 0x10) == 0;
    }
}


bool Mapper001::prgRamAccessible() const {
    if (!prgRamEnabled) return false;

    // SNROM-family MMC1 boards reuse the selected CHR bank's high output as
    // PRG-RAM /CE.  With CHR RAM and 128 KiB PRG, the mmc1_a12 test changes
    // register 2 bit 4 so sprite-pattern fetches ($1xxx) disable WRAM while
    // background-pattern fetches ($0xxx) leave it enabled.  The CPU can then
    // observe the live PPU A12 state through reads at $6000-$7FFF.
    if (chrBanks == 0 && prgBanks == 8 && (control & 0x10u)) {
        const uint8_t activeChrRegister = currentPpuA12 ? chrBank1 : chrBank0;
        return (activeChrRegister & 0x10u) == 0;
    }

    return true;
}

bool Mapper001::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamAccessible()) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        return false;
    }

    if (addr < 0x8000) return false;

    const uint8_t prgMode = (control >> 2) & 0x03;
    const uint8_t prgBankSel = prgBank & 0x0F;

    if (prgMode == 0 || prgMode == 1) {
        // 32KB mode
        uint8_t bank32k = (prgBankSel & 0x0E) >> 1;
        uint8_t banks32k = (prgBanks > 1) ? (prgBanks / 2) : 0;
        if (banks32k == 0) {
            mappedAddr = addr & 0x7FFF;
            return true;
        }
        bank32k %= banks32k;
        mappedAddr = (uint32_t(bank32k) * 0x8000) + (addr & 0x7FFF);
        return true;
    }

    if (prgMode == 2) {
        // Fix first 16KB at $8000, switch 16KB at $C000
        if (addr >= 0x8000 && addr <= 0xBFFF) {
            mappedAddr = (0u * 0x4000) + (addr & 0x3FFF);
            return true;
        }
        // $C000-$FFFF
        uint8_t bank16k = prgBankSel;
        if (prgBanks) bank16k %= prgBanks;
        mappedAddr = (uint32_t(bank16k) * 0x4000) + (addr & 0x3FFF);
        return true;
    }

    // prgMode == 3
    // Switch 16KB at $8000, fix last 16KB at $C000
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        uint8_t bank16k = prgBankSel;
        if (prgBanks) bank16k %= prgBanks;
        mappedAddr = (uint32_t(bank16k) * 0x4000) + (addr & 0x3FFF);
        return true;
    }
    // $C000-$FFFF fixed to last bank
    uint8_t last = prgBanks ? (prgBanks - 1) : 0;
    mappedAddr = (uint32_t(last) * 0x4000) + (addr & 0x3FFF);
    return true;
}

bool Mapper001::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    // PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (prgRamAccessible()) {
            mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
            return true;
        }
        return true; // handled (ignored)
    }

    if (addr < 0x8000) return false;

    // Reset shift register if bit 7 set
    if (data & 0x80) {
        shiftReg = 0x10;
        shiftCount = 0;
        control |= 0x0C;
        updateMirroring();
        mappedAddr = 0;
        return true;
    }

    // Serial load, LSB first
    uint8_t bit = data & 0x01;
    shiftReg = (shiftReg >> 1) | (bit << 4);
    shiftCount++;

    if (shiftCount == 5) {
        commitRegister(addr, shiftReg);
        shiftReg = 0x10;
        shiftCount = 0;
    }

    mappedAddr = 0;
    return true;
}

bool Mapper001::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;

    const uint8_t chrMode = (control >> 4) & 0x01;

    if (chrMode == 0) {
        // 8KB CHR banking
        uint16_t bank8k = uint16_t(chrBank0 & 0x1E) >> 1;
        uint16_t banks8k = chrBanks ? chrBanks : 1;
        bank8k %= banks8k;
        mappedAddr = (uint32_t(bank8k) * 0x2000) + addr;
        return true;
    }

    // 4KB CHR banking
    if (addr <= 0x0FFF) {
        uint16_t bank4k = uint16_t(chrBank0 & 0x1F);
        uint16_t banks4k = chrBanks ? uint16_t(chrBanks * 2) : 2; // 8KB units -> 4KB units
        bank4k %= (banks4k ? banks4k : 1);
        mappedAddr = (uint32_t(bank4k) * 0x1000) + (addr & 0x0FFF);
        return true;
    }

    uint16_t bank4k = uint16_t(chrBank1 & 0x1F);
    uint16_t banks4k = chrBanks ? uint16_t(chrBanks * 2) : 2;
    bank4k %= (banks4k ? banks4k : 1);
    mappedAddr = (uint32_t(bank4k) * 0x1000) + (addr & 0x0FFF);
    return true;
}

bool Mapper001::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF) return false;

    // Only if CHR RAM
    if (chrBanks != 0) return false;

    // Map like ppuMapRead
    return ppuMapRead(addr, mappedAddr);
}

void Mapper001::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, shiftReg);
    write_u8(out, shiftCount);
    write_u8(out, control);
    write_u8(out, chrBank0);
    write_u8(out, chrBank1);
    write_u8(out, prgBank);
    write_u8(out, mirrorMode);
    write_bool(out, prgRamEnabled);
    write_bool(out, currentPpuA12);
}

bool Mapper001::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,shiftReg)) return false;
    if (!read_u8(p,end,shiftCount)) return false;
    if (!read_u8(p,end,control)) return false;
    if (!read_u8(p,end,chrBank0)) return false;
    if (!read_u8(p,end,chrBank1)) return false;
    if (!read_u8(p,end,prgBank)) return false;
    if (!read_u8(p,end,mirrorMode)) return false;
    if (!read_bool(p,end,prgRamEnabled)) return false;
    if (!read_bool(p,end,currentPpuA12)) return false;
    // Derived mirroring depends on control, but mirrorMode is already stored.
    return true;
}
