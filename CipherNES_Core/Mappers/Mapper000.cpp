#include "Mapper000.h"

// -----------------------------------------------------------------------------
// Mapper000 (NROM)
// -----------------------------------------------------------------------------
bool Mapper000::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // Optional PRG-RAM at $6000-$7FFF (present on some NROM boards)
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000) {
        // If 16KB PRG, mirror; if 32KB, map all 32KB
        mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper000::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    // Optional PRG-RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000) {
        // NROM has no PRG banking registers. Writes typically ignored.
        mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper000::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // Pattern table address space always mapped directly in NROM
    if (addr <= 0x1FFF) {
        mappedAddr = addr;
        return true;
    }
    return false;
}

bool Mapper000::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    // If CHR is RAM (chrBanks == 0), writes are allowed; if it's ROM, writes should be ignored
    if (addr <= 0x1FFF) {
        mappedAddr = addr;
        return chrBanks == 0;
    }
    return false;
}
