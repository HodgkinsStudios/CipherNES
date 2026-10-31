#include "Mapper032.h"
#include "../state_io.h"

void Mapper032::reset() {
    prg0 = 0; prg1 = 1; prgMode = 0; mirrorMode = 0xFF;
    for (uint8_t i = 0; i < 8; ++i) chr[i] = i;
}

bool Mapper032::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;
    const uint8_t count = prg8kCount() ? prg8kCount() : 1;
    const uint8_t fixedSecondLast = count >= 2 ? uint8_t(count - 2) : 0;
    const uint8_t fixedLast = count >= 1 ? uint8_t(count - 1) : 0;
    uint8_t bank = 0;
    if (addr <= 0x9FFF) bank = prgMode ? fixedSecondLast : prg0;
    else if (addr <= 0xBFFF) bank = prg1;
    else if (addr <= 0xDFFF) bank = prgMode ? prg0 : fixedSecondLast;
    else bank = fixedLast;
    mappedAddr = uint32_t(bank % count) * 0x2000u + uint32_t(addr & 0x1FFF);
    return true;
}

bool Mapper032::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000) return false;
    if ((addr & 0xF000) == 0x8000) {
        if (addr & 0x0001) {
            prgMode = (data >> 1) & 0x01;
            mirrorMode = (data & 0x01) ? 0 : 1;
        } else {
            prg0 = data;
            if (prg8kCount()) prg0 %= prg8kCount();
        }
    } else if ((addr & 0xF000) == 0x9000) {
        mirrorMode = (data & 0x01) ? 0 : 1;
    } else if ((addr & 0xF000) == 0xA000) {
        prg1 = data;
        if (prg8kCount()) prg1 %= prg8kCount();
    } else if ((addr & 0xF000) == 0xB000) {
        const uint8_t idx = uint8_t(addr & 0x0007);
        chr[idx] = data;
        if (chr1kCount()) chr[idx] %= chr1kCount();
    }
    mappedAddr = 0;
    return true;
}

bool Mapper032::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    if (chrBanks == 0) { mappedAddr = addr; return true; }
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr[slot] % (chr1kCount() ? chr1kCount() : 1)) * 0x400u + uint32_t(addr & 0x03FF);
    return true;
}

bool Mapper032::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF) return false;
    mappedAddr = addr;
    return chrBanks == 0;
}

void Mapper032::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prg0); write_u8(out, prg1); write_u8(out, prgMode); write_u8(out, mirrorMode);
    for (uint8_t i = 0; i < 8; ++i) write_u8(out, chr[i]);
}

bool Mapper032::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, prg0) || !read_u8(p, end, prg1) ||
        !read_u8(p, end, prgMode) || !read_u8(p, end, mirrorMode)) return false;
    for (uint8_t i = 0; i < 8; ++i) if (!read_u8(p, end, chr[i])) return false;
    return true;
}
