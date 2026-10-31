#include "Mapper003.h"

#include "../state_io.h"

// -----------------------------------------------------------------------------
// Mapper003 (CNROM)
// -----------------------------------------------------------------------------
void Mapper003::reset() {
    chrBankSelect = 0;
}

bool Mapper003::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // Optional PRG-RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000) {
        mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper003::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    // Optional PRG-RAM at $6000-$7FFF
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        (void)data;
        return true;
    }

    if (addr >= 0x8000) {
        // Select CHR bank (8KB)
        // CNROM decodes varying numbers of bits depending on board/ROM size.
        // Don't hard-mask to 2 bits; instead wrap to the available CHR bank count.
        chrBankSelect = chrBanks ? uint8_t(data % chrBanks) : 0;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper003::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // 8KB CHR banking
        mappedAddr = (uint32_t(chrBankSelect) * 0x2000) + addr;
        return true;
    }
    return false;
}

bool Mapper003::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        // Only if CHR RAM (rare for CNROM)
        mappedAddr = (uint32_t(chrBankSelect) * 0x2000) + addr;
        return chrBanks == 0;
    }
    return false;
}

void Mapper003::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, chrBankSelect);
}

bool Mapper003::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, chrBankSelect);
}
