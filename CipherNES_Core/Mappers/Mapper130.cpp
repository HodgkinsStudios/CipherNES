#include "Mapper130.h"
#include "../state_io.h"
#include <algorithm>

Mapper130::Mapper130(uint16_t p, uint16_t c) : Mapper(p, c) { reset(); }

void Mapper130::reset() {
    regs[0] = regs[1] = 0;
    mode = 0;
    updateState();
}

void Mapper130::updateState() {
    const uint16_t outer = uint16_t(mode & 0x03u) << 3;
    chr4[0] = uint16_t((regs[0] >> 3) | (outer << 2));
    chr4[1] = uint16_t((regs[1] >> 3) | (outer << 2));

    if (mode & 0x08u) {
        const uint16_t base = uint16_t(outer | (regs[0] & 0x06u));
        prg16[0] = base;
        prg16[1] = uint16_t(base + 1u);
    } else {
        prg16[0] = uint16_t(outer | (regs[0] & 0x07u));
        prg16[1] = uint16_t(outer | 0x07u);
    }
    mirrorMode = (mode & 0x04u) ? 0u : 1u;
}

bool Mapper130::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint32_t count = std::max<uint32_t>(1u, prgBanks);
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 14);
    mappedAddr = (uint32_t(prg16[slot]) % count) * 0x4000u + uint32_t(addr & 0x3FFFu);
    return true;
}

bool Mapper130::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000u) return false;
    switch (addr & 0xE000u) {
        case 0xA000u: regs[0] = data; break;
        case 0xC000u: regs[1] = data; break;
        case 0xE000u: mode = uint8_t(data & 0x0Fu); break;
        default: mappedAddr = 0; return true;
    }
    updateState();
    mappedAddr = 0;
    return true;
}

bool Mapper130::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint32_t count = std::max<uint32_t>(1u, chrBanks ? uint32_t(chrBanks) * 2u : 2u);
    const uint8_t slot = uint8_t(addr >> 12);
    mappedAddr = (uint32_t(chr4[slot]) % count) * 0x1000u + uint32_t(addr & 0x0FFFu);
    return true;
}

bool Mapper130::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    if (chrBanks != 0u || addr > 0x1FFFu) return false;
    return ppuMapRead(addr, mappedAddr);
}

void Mapper130::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, regs[0]); write_u8(out, regs[1]); write_u8(out, mode);
    write_u8(out, mirrorMode);
}

bool Mapper130::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,regs[0]) || !read_u8(p,end,regs[1]) ||
        !read_u8(p,end,mode) || !read_u8(p,end,mirrorMode)) return false;
    updateState();
    return true;
}
