#include "Mapper078.h"
#include "../state_io.h"

void Mapper078::reset() { prgBank = 0; chrBank = 0; mirrorMode = 0xFF; }

bool Mapper078::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
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

bool Mapper078::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        prgBank = data & 0x07;
        if (prgBanks) prgBank %= prgBanks;
        chrBank = (data >> 4) & 0x0F;
        chrBank = chrBanks ? uint8_t(chrBank % chrBanks) : 0;
        mirrorMode = (data & 0x08) ? 1 : 0;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper078::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr);
        return true;
    }
    return false;
}

bool Mapper078::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr); return chrBanks == 0; }
    return false;
}

void Mapper078::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgBank); write_u8(out, chrBank); write_u8(out, mirrorMode);
}

bool Mapper078::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, prgBank) && read_u8(p, end, chrBank) && read_u8(p, end, mirrorMode);
}
