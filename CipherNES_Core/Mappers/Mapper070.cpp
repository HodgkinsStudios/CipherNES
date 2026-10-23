#include "Mapper070.h"
#include "../state_io.h"

void Mapper070::reset() { prgBank = 0; chrBank = 0; }

bool Mapper070::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    uint16_t bank = (addr < 0xC000u) ? uint16_t(prgBank) : uint16_t(prgBanks ? prgBanks - 1u : 0u);
    if (prgBanks) bank %= prgBanks;
    mappedAddr = uint32_t(bank) * 0x4000u + uint32_t(addr & 0x3FFFu);
    return true;
}

bool Mapper070::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000u) return false;
    prgBank = uint8_t((data >> 4) & 0x07u);
    chrBank = uint8_t(data & 0x0Fu);
    mappedAddr = 0;
    return true;
}

bool Mapper070::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? chrBanks : 1u;
    mappedAddr = uint32_t(uint16_t(chrBank) % count) * 0x2000u + addr;
    return true;
}

bool Mapper070::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}

void Mapper070::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, prgBank);
    ciphernes_state::write_u8(out, chrBank);
}
bool Mapper070::loadState(const uint8_t*& p, const uint8_t* end) {
    return ciphernes_state::read_u8(p, end, prgBank) && ciphernes_state::read_u8(p, end, chrBank);
}
