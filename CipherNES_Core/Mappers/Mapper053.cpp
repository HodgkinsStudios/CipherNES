#include "Mapper053.h"
#include "../state_io.h"
#include <algorithm>

Mapper053::Mapper053(uint16_t p, uint16_t c, bool menuEpromFirst)
    : Mapper(p, c), prgPages8k(std::max<uint32_t>(1u, uint32_t(p) * 2u)), epromFirst(menuEpromFirst) {
    reset();
}

void Mapper053::reset() {
    regs[0] = regs[1] = 0;
    updateState();
}

void Mapper053::updateState() {
    const uint16_t r = uint16_t((uint16_t(regs[0]) << 3) & 0x78u);

    // $6000-$7FFF is the final 8 KiB page of the selected 128 KiB block.
    prgPage[0] = uint16_t((r << 1) | 0x0Fu);
    if (epromFirst) prgPage[0] = uint16_t(prgPage[0] + 4u);

    uint16_t lower16;
    uint16_t upper16;
    if (regs[0] & 0x10u) {
        const uint16_t orderOffset = epromFirst ? 2u : 0u;
        lower16 = uint16_t((r | (regs[1] & 0x07u)) + orderOffset);
        upper16 = uint16_t((r | 0x07u) + orderOffset);
    } else if (epromFirst) {
        lower16 = 0x00u;
        upper16 = 0x01u;
    } else {
        lower16 = 0x80u;
        upper16 = 0x81u;
    }

    prgPage[1] = uint16_t(lower16 * 2u);
    prgPage[2] = uint16_t(lower16 * 2u + 1u);
    prgPage[3] = uint16_t(upper16 * 2u);
    prgPage[4] = uint16_t(upper16 * 2u + 1u);

    mirrorMode = (regs[0] & 0x20u) ? 0u : 1u;
}

bool Mapper053::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    uint8_t slot;
    if (addr >= 0x6000u && addr <= 0x7FFFu) slot = 0;
    else if (addr >= 0x8000u) slot = uint8_t(1u + ((addr - 0x8000u) >> 13));
    else return false;

    const uint32_t page = uint32_t(prgPage[slot]) % prgPages8k;
    mappedAddr = page * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper053::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x6000u) return false;
    regs[addr >= 0x8000u ? 1 : 0] = data;
    updateState();
    mappedAddr = 0;
    return true;
}

bool Mapper053::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}

bool Mapper053::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}

void Mapper053::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, regs[0]);
    write_u8(out, regs[1]);
    write_bool(out, epromFirst);
    write_u8(out, mirrorMode);
}

bool Mapper053::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, regs[0]) || !read_u8(p, end, regs[1]) ||
        !read_bool(p, end, epromFirst) || !read_u8(p, end, mirrorMode)) return false;
    updateState();
    return true;
}
