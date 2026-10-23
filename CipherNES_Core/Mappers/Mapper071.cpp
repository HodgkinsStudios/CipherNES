#include "Mapper071.h"
#include "../state_io.h"

void Mapper071::reset() { prgBank = 0; mirrorMode = 0xFF; }

bool Mapper071::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mappedAddr = uint32_t(prgBank % (prgBanks ? prgBanks : 1)) * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        const uint8_t bank = prgBanks ? uint8_t(prgBanks - 1) : 0;
        mappedAddr = uint32_t(bank) * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper071::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x9000 && addr <= 0x9FFF) {
        mirrorMode = (data & 0x10) ? 4 : 3;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xC000) {
        prgBank = data & 0x0F;
        if (prgBanks) prgBank %= prgBanks;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0x8000) {
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper071::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return true; }
    return false;
}

bool Mapper071::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return chrBanks == 0; }
    return false;
}

void Mapper071::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, prgBank);
    ciphernes_state::write_u8(out, mirrorMode);
}

bool Mapper071::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, prgBank) && read_u8(p, end, mirrorMode);
}
