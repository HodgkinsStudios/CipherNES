#include "Mapper193.h"

#include "../state_io.h"

void Mapper193::reset() {
    prg = 0;
    for (uint16_t& value : chr2k) value = 0;
}

bool Mapper193::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;

    const uint16_t count = prgBanks ? uint16_t(prgBanks * 2u) : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000) >> 13);
    uint16_t page = prg;
    if (slot != 0) {
        // Slots 1..3 are fixed to the last three 8 KiB pages. Clamp for tiny
        // malformed images rather than allowing unsigned underflow.
        const uint16_t distanceFromEnd = uint16_t(4u - slot);
        page = count > distanceFromEnd ? uint16_t(count - distanceFromEnd) : 0u;
    }

    mappedAddr = uint32_t(page % count) * 0x2000u + (addr & 0x1FFFu);
    return true;
}

bool Mapper193::cpuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t data) {
    if (addr < 0x6000 || addr > 0x7FFF) return false;

    switch (addr & 0x03) {
        case 0:
            chr2k[0] = uint16_t(data >> 1);
            chr2k[1] = uint16_t((data >> 1) + 1);
            break;
        case 1:
            chr2k[2] = uint16_t(data >> 1);
            break;
        case 2:
            chr2k[3] = uint16_t(data >> 1);
            break;
        case 3:
            prg = data;
            break;
    }

    mappedAddr = 0;
    return true;
}

bool Mapper193::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;

    const uint16_t count = chrBanks ? uint16_t(chrBanks * 4u) : 4u;
    const uint8_t slot = uint8_t(addr >> 11);
    mappedAddr = uint32_t(chr2k[slot] % count) * 0x0800u + (addr & 0x07FFu);
    return true;
}

bool Mapper193::ppuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t /*data*/) {
    return chrBanks == 0 && ppuMapRead(addr, mappedAddr);
}

void Mapper193::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prg);
    for (uint16_t value : chr2k) write_u16(out, value);
}

bool Mapper193::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, prg)) return false;
    for (uint16_t& value : chr2k) {
        if (!read_u16(p, end, value)) return false;
    }
    return true;
}
