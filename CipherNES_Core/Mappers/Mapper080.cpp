#include "Mapper080.h"

#include "../state_io.h"

void Mapper080::reset() {
    prg[0] = 0;
    prg[1] = 1;
    prg[2] = 2;
    for (uint8_t& value : chr) value = 0;
    ramPermission = 0;
    mirrorMode = 0xFF;
}

bool Mapper080::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x7F00 && addr <= 0x7FFF) {
        if (ramPermission != 0xA3) return false;
        // The physical X1-005 RAM is 128 bytes, mirrored once in this range.
        mappedAddr = PRG_RAM_FLAG | uint32_t((addr - 0x7F00) & 0x7F);
        return true;
    }
    if (addr < 0x8000) return false;

    const uint16_t count = prgBanks ? uint16_t(prgBanks * 2u) : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000) >> 13);
    const uint16_t page = slot < 3 ? prg[slot] : uint16_t(count - 1);
    mappedAddr = uint32_t(page % count) * 0x2000u + (addr & 0x1FFFu);
    return true;
}

bool Mapper080::cpuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t data) {
    if (addr >= 0x7F00 && addr <= 0x7FFF) {
        if (ramPermission != 0xA3) return false;
        mappedAddr = PRG_RAM_FLAG | uint32_t((addr - 0x7F00) & 0x7F);
        return true;
    }
    if (addr < 0x7EF0 || addr > 0x7EFF) return false;

    switch (addr) {
        case 0x7EF0:
            chr[0] = data;
            chr[1] = uint8_t(data + 1);
            break;
        case 0x7EF1:
            chr[2] = data;
            chr[3] = uint8_t(data + 1);
            break;
        case 0x7EF2: chr[4] = data; break;
        case 0x7EF3: chr[5] = data; break;
        case 0x7EF4: chr[6] = data; break;
        case 0x7EF5: chr[7] = data; break;
        case 0x7EF6:
        case 0x7EF7:
            mirrorMode = (data & 1) ? 1 : 0;
            break;
        case 0x7EF8:
        case 0x7EF9:
            ramPermission = data;
            break;
        case 0x7EFA:
        case 0x7EFB:
            prg[0] = data;
            break;
        case 0x7EFC:
        case 0x7EFD:
            prg[1] = data;
            break;
        case 0x7EFE:
        case 0x7EFF:
            prg[2] = data;
            break;
    }

    mappedAddr = 0;
    return true;
}

bool Mapper080::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    const uint16_t count = chrBanks ? uint16_t(chrBanks * 8u) : 8u;
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr[slot] % count) * 0x0400u + (addr & 0x03FFu);
    return true;
}

bool Mapper080::ppuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t /*data*/) {
    return chrBanks == 0 && ppuMapRead(addr, mappedAddr);
}

void Mapper080::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint8_t value : prg) write_u8(out, value);
    for (uint8_t value : chr) write_u8(out, value);
    write_u8(out, ramPermission);
    write_u8(out, mirrorMode);
}

bool Mapper080::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint8_t& value : prg) {
        if (!read_u8(p, end, value)) return false;
    }
    for (uint8_t& value : chr) {
        if (!read_u8(p, end, value)) return false;
    }
    return read_u8(p, end, ramPermission)
        && read_u8(p, end, mirrorMode);
}
