#include "Mapper068.h"
#include "../state_io.h"

void Mapper068::reset() {
    for (uint16_t& v : chr2k) v = 0;
    nt1k[0] = nt1k[1] = 0x80u;
    prgPage = 0;
    mirrorMode = 1;
    useChrNametables = false;
    prgRamEnabled = false;
    usingExternalRom = false;
    licensingTimer = 0;
}

uint8_t Mapper068::ntSelect(uint8_t table) const {
    switch (mirrorMode) {
        case 1: return uint8_t(table & 1u);              // vertical
        case 0: return uint8_t((table >> 1) & 1u);      // horizontal
        case 3: return 0;
        case 4: return 1;
        default: return uint8_t(table & 1u);
    }
}

bool Mapper068::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if (!prgRamEnabled) return false;
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;
    if (addr < 0xC000u) {
        if (usingExternalRom && licensingTimer == 0) return false;
        const uint16_t count = prgBanks ? prgBanks : 1u;
        mappedAddr = uint32_t(prgPage % count) * 0x4000u + uint32_t(addr & 0x3FFFu);
    } else {
        const uint16_t count = prgBanks ? prgBanks : 1u;
        const uint16_t fixed = uint16_t((count > 7u ? 7u : count - 1u) % count);
        mappedAddr = uint32_t(fixed) * 0x4000u + uint32_t(addr & 0x3FFFu);
    }
    return true;
}

bool Mapper068::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000u && addr <= 0x7FFFu) {
        if (!prgRamEnabled) return false;
        licensingTimer = 1024u * 105u;
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;
    switch (addr & 0xF000u) {
        case 0x8000u: chr2k[0] = data; break;
        case 0x9000u: chr2k[1] = data; break;
        case 0xA000u: chr2k[2] = data; break;
        case 0xB000u: chr2k[3] = data; break;
        case 0xC000u: nt1k[0] = uint16_t(data | 0x80u); break;
        case 0xD000u: nt1k[1] = uint16_t(data | 0x80u); break;
        case 0xE000u:
            switch (data & 3u) {
                case 0: mirrorMode = 1; break;
                case 1: mirrorMode = 0; break;
                case 2: mirrorMode = 3; break;
                default: mirrorMode = 4; break;
            }
            useChrNametables = (data & 0x10u) != 0;
            break;
        case 0xF000u: {
            const uint16_t count = prgBanks ? prgBanks : 1u;
            usingExternalRom = ((data & 0x08u) == 0u) && count > 8u;
            if (usingExternalRom) prgPage = uint16_t(8u + ((data & 7u) % (count - 8u)));
            else prgPage = uint16_t(data & 7u);
            prgRamEnabled = (data & 0x10u) != 0;
            break;
        }
    }
    mappedAddr = 0;
    return true;
}

bool Mapper068::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    const uint16_t a = uint16_t(addr & 0x3FFFu);
    if (a <= 0x1FFFu) {
        const uint16_t count = chrBanks ? uint16_t(chrBanks * 4u) : 4u;
        const uint8_t slot = uint8_t(a >> 11);
        mappedAddr = uint32_t(chr2k[slot] % count) * 0x0800u + uint32_t(a & 0x07FFu);
        return true;
    }
    if (a >= 0x2000u && a <= 0x3EFFu && useChrNametables) {
        const uint16_t folded = uint16_t((a - 0x2000u) & 0x0FFFu);
        const uint8_t table = uint8_t(folded >> 10);
        const uint16_t count = chrBanks ? uint16_t(chrBanks * 8u) : 8u;
        const uint16_t page = uint16_t(nt1k[ntSelect(table)] % count);
        mappedAddr = uint32_t(page) * 0x0400u + uint32_t(folded & 0x03FFu);
        return true;
    }
    return false;
}

bool Mapper068::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0) return false;
    return ppuMapRead(addr, mappedAddr);
}
void Mapper068::cpuClock() {
    if (licensingTimer) --licensingTimer;
}

void Mapper068::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint16_t value : chr2k) write_u16(out, value);
    write_u16(out, nt1k[0]);
    write_u16(out, nt1k[1]);
    write_u16(out, prgPage);
    write_u8(out, mirrorMode);
    write_bool(out, useChrNametables);
    write_bool(out, prgRamEnabled);
    write_bool(out, usingExternalRom);
    write_u32(out, licensingTimer);
}
bool Mapper068::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint16_t& value : chr2k) {
        if (!read_u16(p, end, value)) return false;
    }
    return read_u16(p, end, nt1k[0])
        && read_u16(p, end, nt1k[1])
        && read_u16(p, end, prgPage)
        && read_u8(p, end, mirrorMode)
        && read_bool(p, end, useChrNametables)
        && read_bool(p, end, prgRamEnabled)
        && read_bool(p, end, usingExternalRom)
        && read_u32(p, end, licensingTimer);
}
