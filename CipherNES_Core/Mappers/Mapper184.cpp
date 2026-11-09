#include "Mapper184.h"
#include "../state_io.h"

void Mapper184::reset() { chrBank[0] = 0; chrBank[1] = 0x80u; }

bool Mapper184::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint32_t bytes = uint32_t(prgBanks ? prgBanks : 1u) * 0x4000u;
    mappedAddr = uint32_t(addr - 0x8000u) % bytes;
    return true;
}

bool Mapper184::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        chrBank[0] = uint16_t(data & 0x07u);
        chrBank[1] = uint16_t(0x80u | ((data >> 4) & 0x07u));
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0x8000u) { mappedAddr = 0; return true; }
    return false;
}

bool Mapper184::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? uint16_t(chrBanks * 2u) : 2u;
    const uint8_t slot = uint8_t(addr >> 12);
    mappedAddr = uint32_t(chrBank[slot] % count) * 0x1000u + uint32_t(addr & 0x0FFFu);
    return true;
}
bool Mapper184::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    return ppuMapRead(addr, mappedAddr);
}
void Mapper184::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u16(out, chrBank[0]); ciphernes_state::write_u16(out, chrBank[1]);
}
bool Mapper184::loadState(const uint8_t*& p, const uint8_t* end) {
    return ciphernes_state::read_u16(p, end, chrBank[0]) && ciphernes_state::read_u16(p, end, chrBank[1]);
}
