#include "Mapper185.h"
#include "../state_io.h"

void Mapper185::reset() { chrBank = 0; }

bool Mapper185::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) { mappedAddr = addr & (prgBanks > 1 ? 0x7FFF : 0x3FFF); return true; }
    return false;
}

bool Mapper185::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        chrBank = data;
        chrBank = chrBanks ? uint8_t(chrBank % chrBanks) : 0;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper185::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) { mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr); return true; }
    return false;
}

bool Mapper185::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = chrBanks ? (uint32_t(chrBank) * 0x2000u + addr) : uint32_t(addr); return chrBanks == 0; }
    return false;
}

void Mapper185::saveState(std::vector<uint8_t>& out) const { ciphernes_state::write_u8(out, chrBank); }
bool Mapper185::loadState(const uint8_t*& p, const uint8_t* end) { return ciphernes_state::read_u8(p, end, chrBank); }
