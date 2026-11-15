#include "Mapper180.h"
#include "../state_io.h"

void Mapper180::reset() { prgBank = 0; }

bool Mapper180::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mappedAddr = uint32_t(addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        mappedAddr = uint32_t(prgBank % (prgBanks ? prgBanks : 1)) * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper180::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        prgBank = data & 0x07;
        if (prgBanks) prgBank %= prgBanks;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper180::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return true; }
    return false;
}

bool Mapper180::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return chrBanks == 0; }
    return false;
}

void Mapper180::saveState(std::vector<uint8_t>& out) const { ciphernes_state::write_u8(out, prgBank); }
bool Mapper180::loadState(const uint8_t*& p, const uint8_t* end) { return ciphernes_state::read_u8(p, end, prgBank); }
