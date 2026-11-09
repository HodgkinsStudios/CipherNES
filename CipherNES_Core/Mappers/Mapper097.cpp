#include "Mapper097.h"
#include "../state_io.h"

void Mapper097::reset() { prgBank = 0; mirrorMode = 0xFF; }

bool Mapper097::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
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

bool Mapper097::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        prgBank = data & 0x0F;
        if (prgBanks) prgBank %= prgBanks;
        mirrorMode = (data & 0x80) ? 0 : 1;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper097::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return true; }
    return false;
}

bool Mapper097::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return chrBanks == 0; }
    return false;
}

void Mapper097::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, prgBank);
    ciphernes_state::write_u8(out, mirrorMode);
}

bool Mapper097::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, prgBank) && read_u8(p, end, mirrorMode);
}
