#include "Mapper075.h"
#include "../state_io.h"

void Mapper075::reset() {
    prg[0] = 0; prg[1] = 1; prg[2] = 2;
    chr[0] = chr[1] = 0;
    mirrorMode = 0xFF;
}

bool Mapper075::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint16_t count = prgBanks ? uint16_t(prgBanks * 2u) : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    const uint16_t page = slot < 3u ? uint16_t(prg[slot] % count) : uint16_t(count - 1u);
    mappedAddr = uint32_t(page) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool Mapper075::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000u) return false;
    switch (addr & 0xF000u) {
        case 0x8000u: prg[0] = data; break;
        case 0x9000u:
            mirrorMode = (data & 1u) ? 0u : 1u;
            chr[0] = uint8_t((chr[0] & 0x0Fu) | ((data & 0x02u) << 3));
            chr[1] = uint8_t((chr[1] & 0x0Fu) | ((data & 0x04u) << 2));
            break;
        case 0xA000u: prg[1] = data; break;
        case 0xC000u: prg[2] = data; break;
        case 0xE000u: chr[0] = uint8_t((chr[0] & 0x10u) | (data & 0x0Fu)); break;
        case 0xF000u: chr[1] = uint8_t((chr[1] & 0x10u) | (data & 0x0Fu)); break;
        default: break;
    }
    mappedAddr = 0;
    return true;
}

bool Mapper075::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? uint16_t(chrBanks * 2u) : 2u;
    const uint8_t slot = uint8_t(addr >> 12);
    mappedAddr = uint32_t(uint16_t(chr[slot]) % count) * 0x1000u + uint32_t(addr & 0x0FFFu);
    return true;
}

bool Mapper075::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    return ppuMapRead(addr, mappedAddr);
}

void Mapper075::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint8_t v : prg) write_u8(out, v);
    for (uint8_t v : chr) write_u8(out, v);
    write_u8(out, mirrorMode);
}
bool Mapper075::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint8_t& v : prg) if (!read_u8(p, end, v)) return false;
    for (uint8_t& v : chr) if (!read_u8(p, end, v)) return false;
    return read_u8(p, end, mirrorMode);
}
