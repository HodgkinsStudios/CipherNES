#include "Mapper113.h"
#include "../state_io.h"

void Mapper113::reset() { prgBank = 0; chrBank = 0; mirrorMode = 0; }

bool Mapper113::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint16_t count32 = prgBanks ? uint16_t((prgBanks + 1u) / 2u) : 1u;
    mappedAddr = uint32_t(uint16_t(prgBank) % count32) * 0x8000u + uint32_t(addr & 0x7FFFu);
    return true;
}

bool Mapper113::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if ((addr & 0xE100u) == 0x4100u) {
        prgBank = uint8_t((data >> 3) & 0x07u);
        chrBank = uint8_t((data & 0x07u) | ((data >> 3) & 0x08u));
        mirrorMode = (data & 0x80u) ? 1u : 0u;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0x8000u) { mappedAddr = 0; return true; }
    return false;
}

bool Mapper113::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? chrBanks : 1u;
    mappedAddr = uint32_t(uint16_t(chrBank) % count) * 0x2000u + addr;
    return true;
}
bool Mapper113::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}
void Mapper113::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, prgBank); ciphernes_state::write_u8(out, chrBank); ciphernes_state::write_u8(out, mirrorMode);
}
bool Mapper113::loadState(const uint8_t*& p, const uint8_t* end) {
    return ciphernes_state::read_u8(p, end, prgBank) && ciphernes_state::read_u8(p, end, chrBank) && ciphernes_state::read_u8(p, end, mirrorMode);
}
