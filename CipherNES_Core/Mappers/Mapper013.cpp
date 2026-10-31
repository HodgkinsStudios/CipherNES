#include "Mapper013.h"

#include "../state_io.h"

bool Mapper013::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;
    const uint32_t prgBytes = uint32_t(prgBanks ? prgBanks : 1u) * 0x4000u;
    mappedAddr = uint32_t(addr - 0x8000u) % prgBytes;
    return true;
}

bool Mapper013::cpuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t data) {
    if (addr < 0x8000) return false;
    upperChr4 = uint8_t(data & 0x03u);
    mappedAddr = 0;
    return true;
}

bool Mapper013::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    if (addr < 0x1000) {
        mappedAddr = addr;
    } else {
        mappedAddr = uint32_t(upperChr4) * 0x1000u + (addr & 0x0FFFu);
    }
    return true;
}

bool Mapper013::ppuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t /*data*/) {
    return ppuMapRead(addr, mappedAddr);
}

void Mapper013::saveState(std::vector<uint8_t>& out) const {
    ciphernes_state::write_u8(out, upperChr4);
}

bool Mapper013::loadState(const uint8_t*& p, const uint8_t* end) {
    return ciphernes_state::read_u8(p, end, upperChr4);
}
