#include "Mapper087.h"
#include "../state_io.h"

void Mapper087::reset() { chrBank = 0; }

bool Mapper087::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
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

bool Mapper087::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        chrBank = uint8_t(((data & 0x02) >> 1) | ((data & 0x01) << 1));
        chrBank = chrBanks ? uint8_t(chrBank % chrBanks) : 0;
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0x8000) { mappedAddr = 0; return true; }
    return false;
}

bool Mapper087::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr);
        return true;
    }
    return false;
}

bool Mapper087::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr);
        return chrBanks == 0;
    }
    return false;
}

void Mapper087::saveState(std::vector<uint8_t>& out) const { ciphernes_state::write_u8(out, chrBank); }
bool Mapper087::loadState(const uint8_t*& p, const uint8_t* end) { return ciphernes_state::read_u8(p, end, chrBank); }
