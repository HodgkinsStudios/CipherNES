#include "Mapper232.h"
#include "../state_io.h"

void Mapper232::reset() { outerBank = 0; innerBank = 0; }

bool Mapper232::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;
    const uint8_t banks16k = prgBanks ? prgBanks : 1;
    const uint8_t base = uint8_t((outerBank << 1) % banks16k);
    uint8_t bank = base;
    if (addr >= 0xC000) bank = uint8_t((base | innerBank) % banks16k);
    mappedAddr = uint32_t(bank) * 0x4000u + uint32_t(addr & 0x3FFF);
    return true;
}

bool Mapper232::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        outerBank = (data >> 3) & 0x03;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xC000) {
        innerBank = data & 0x03;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper232::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return true; }
    return false;
}

bool Mapper232::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) { mappedAddr = addr; return chrBanks == 0; }
    return false;
}

void Mapper232::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, outerBank);
    ciphernes_state::write_u8(out, innerBank);
}

bool Mapper232::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, outerBank) && read_u8(p, end, innerBank);
}
