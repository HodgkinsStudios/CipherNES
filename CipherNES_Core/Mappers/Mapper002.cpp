#include "Mapper002.h"

#include "../state_io.h"

// -----------------------------------------------------------------------------
// Mapper002 (UxROM)
// -----------------------------------------------------------------------------
void Mapper002::reset() {
    prgBankSelect = 0;
}

bool Mapper002::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // PRG-RAM at $6000-$7FFF is common on UxROM boards
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000 && addr <= 0xBFFF) {
        // Switchable 16KB bank
        mappedAddr = (uint32_t(prgBankSelect) * 0x4000) + (addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        // Fixed to last 16KB bank
        uint8_t lastBank = (prgBanks > 0) ? (prgBanks - 1) : 0;
        mappedAddr = (uint32_t(lastBank) * 0x4000) + (addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper002::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    // PRG-RAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        (void)data;
        return true;
    }

    if (addr >= 0x8000) {
        // Bank select is lower bits; mask to available banks
        prgBankSelect = data & 0x0F;
        if (prgBanks) prgBankSelect %= prgBanks;
        mappedAddr = 0; // no direct memory write
        return true;
    }
    return false;
}

bool Mapper002::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        mappedAddr = addr;
        return true;
    }
    return false;
}

bool Mapper002::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        mappedAddr = addr;
        return chrBanks == 0; // CHR RAM only
    }
    return false;
}

void Mapper002::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgBankSelect);
}

bool Mapper002::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, prgBankSelect);
}
