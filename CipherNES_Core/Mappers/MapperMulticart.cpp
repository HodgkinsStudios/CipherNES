#include "MapperMulticart.h"
#include "../state_io.h"

namespace {
static const uint8_t kPrg244[4][4] = {
    {0,1,2,3}, {3,2,1,0}, {0,2,1,3}, {3,1,2,0}
};
static const uint8_t kChr244[8][8] = {
    {0,1,2,3,4,5,6,7}, {0,2,1,3,4,6,5,7},
    {0,1,4,5,2,3,6,7}, {0,4,1,5,2,6,3,7},
    {0,4,2,6,1,5,3,7}, {0,2,4,6,1,3,5,7},
    {7,6,5,4,3,2,1,0}, {7,6,5,4,3,2,1,0}
};
}

MapperMulticart::MapperMulticart(uint16_t prgBanks, uint16_t chrBanks, uint16_t mapper)
    : Mapper(prgBanks, chrBanks), mapperNumber(mapper) {
    reset(true);
}

uint32_t MapperMulticart::capabilities() const {
    switch (mapperNumber) {
        case 225: case 226: case 227: case 228: case 229:
        case 230: case 231: case 233: case 255:
            return MAPPER_CAP_MIRRORING;
        default:
            return 0;
    }
}

void MapperMulticart::set32k(uint16_t page32k) {
    prgPage[0] = uint16_t(page32k * 2u);
    prgPage[1] = uint16_t(page32k * 2u + 1u);
}

void MapperMulticart::reset() { reset(true); }

void MapperMulticart::reset(bool hardReset) {
    regs[0] = regs[1] = 0;
    prgPage[0] = 0;
    prgPage[1] = 1;
    chrPage = 0;
    mirrorMode = 0xFF;
    suppressInitialReset = false;

    if (mapperNumber == 230u) {
        if (hardReset || !resetSeen) {
            resetToggle = true;
            resetSeen = true;
        } else {
            resetToggle = !resetToggle;
        }
        if (resetToggle) {
            prgPage[0] = 0; prgPage[1] = 7; mirrorMode = 1;
        } else {
            prgPage[0] = 8; prgPage[1] = 9; mirrorMode = 0;
        }
    } else if (mapperNumber == 233u) {
        if (hardReset || !resetSeen) {
            resetToggle = false;
            resetSeen = true;
        } else {
            resetToggle = !resetToggle;
        }
        update226Style(true);
    }
}

void MapperMulticart::update226Style(bool mapper233Mode) {
    uint16_t page = uint16_t(regs[0] & 0x1Fu);
    if (mapper233Mode) page |= uint16_t(resetToggle ? 0x20u : 0u);
    else page |= uint16_t((regs[0] & 0x80u) >> 2);
    page |= uint16_t((regs[1] & 1u) << 6);
    if (regs[0] & 0x20u) prgPage[0] = prgPage[1] = page;
    else { prgPage[0] = uint16_t(page & 0xFEu); prgPage[1] = uint16_t((page & 0xFEu) + 1u); }
    mirrorMode = (regs[0] & 0x40u) ? 1u : 0u;
}

void MapperMulticart::writeLatch(uint16_t addr, uint8_t data) {
    switch (mapperNumber) {
        case 225: {
            const uint16_t high = uint16_t((addr >> 8) & 0x40u);
            const uint16_t page = uint16_t(((addr >> 6) & 0x3Fu) | high);
            if (addr & 0x1000u) prgPage[0] = prgPage[1] = page;
            else { prgPage[0] = uint16_t(page & 0xFEu); prgPage[1] = uint16_t((page & 0xFEu) + 1u); }
            chrPage = uint16_t((addr & 0x3Fu) | high);
            mirrorMode = (addr & 0x2000u) ? 0u : 1u;
            break;
        }
        case 226:
        case 233:
            regs[addr & 1u] = data;
            update226Style(mapperNumber == 233u);
            break;
        case 227: {
            const uint16_t page = uint16_t(((addr >> 2) & 0x1Fu) | ((addr & 0x100u) >> 3));
            const bool s = (addr & 1u) != 0;
            const bool l = (addr & 0x200u) != 0;
            const bool mode = (addr & 0x80u) != 0;
            if (mode) {
                if (s) { prgPage[0] = uint16_t(page & 0xFEu); prgPage[1] = uint16_t((page & 0xFEu) + 1u); }
                else prgPage[0] = prgPage[1] = page;
            } else if (s) {
                prgPage[0] = uint16_t(page & 0x3Eu);
                prgPage[1] = l ? uint16_t(page | 0x07u) : uint16_t(page & 0x38u);
            } else {
                prgPage[0] = page;
                prgPage[1] = l ? uint16_t(page | 0x07u) : uint16_t(page & 0x38u);
            }
            mirrorMode = (addr & 2u) ? 0u : 1u;
            break;
        }
        case 228: {
            uint16_t chip = uint16_t((addr >> 11) & 3u);
            if (chip == 3u) chip = 2u;
            const uint16_t page = uint16_t(((addr >> 6) & 0x1Fu) | (chip << 5));
            if (addr & 0x20u) prgPage[0] = prgPage[1] = page;
            else { prgPage[0] = uint16_t(page & 0xFEu); prgPage[1] = uint16_t((page & 0xFEu) + 1u); }
            chrPage = uint16_t(((addr & 0x0Fu) << 2) | (data & 3u));
            mirrorMode = (addr & 0x2000u) ? 0u : 1u;
            break;
        }
        case 229:
            chrPage = uint16_t(addr & 0xFFu);
            if ((addr & 0x1Eu) == 0u) { prgPage[0] = 0; prgPage[1] = 1; }
            else prgPage[0] = prgPage[1] = uint16_t(addr & 0x1Fu);
            mirrorMode = (addr & 0x20u) ? 0u : 1u;
            break;
        case 230:
            if (resetToggle) {
                prgPage[0] = uint16_t(data & 7u);
            } else {
                const uint16_t page = uint16_t((data & 0x1Fu) + 8u);
                if (data & 0x20u) prgPage[0] = prgPage[1] = page;
                else { prgPage[0] = uint16_t((data & 0x1Eu) + 8u); prgPage[1] = uint16_t((data & 0x1Eu) + 9u); }
                mirrorMode = (data & 0x40u) ? 1u : 0u;
            }
            break;
        case 231: {
            const uint16_t page = uint16_t(((addr >> 5) & 1u) | (addr & 0x1Eu));
            prgPage[0] = uint16_t(page & 0x1Eu);
            prgPage[1] = page;
            mirrorMode = (addr & 0x80u) ? 0u : 1u;
            break;
        }
        case 240:
            set32k(uint16_t((data >> 4) & 0x0Fu));
            chrPage = uint16_t(data & 0x0Fu);
            break;
        case 241:
            set32k(data);
            break;
        case 244:
            if (data & 0x08u) chrPage = kChr244[(data >> 4) & 7u][data & 7u];
            else set32k(kPrg244[(data >> 4) & 3u][data & 3u]);
            break;
        case 255: {
            const uint16_t bit = (addr & 0x1000u) ? 0u : 1u;
            const uint16_t page = uint16_t(((addr >> 8) & 0x40u) | ((addr >> 6) & 0x3Fu));
            prgPage[0] = uint16_t(page & ~bit);
            prgPage[1] = uint16_t(page | bit);
            chrPage = uint16_t(((addr >> 8) & 0x40u) | (addr & 0x3Fu));
            mirrorMode = (addr & 0x2000u) ? 0u : 1u;
            break;
        }
        default: break;
    }
}

bool MapperMulticart::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    const uint16_t count = prgBanks ? prgBanks : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 14);
    mappedAddr = uint32_t(prgPage[slot] % count) * 0x4000u + uint32_t(addr & 0x3FFFu);
    return true;
}

bool MapperMulticart::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    bool handled = false;
    if (mapperNumber == 240u) handled = addr >= 0x4020u && addr <= 0x5FFFu;
    else handled = addr >= 0x8000u;
    if (!handled) return false;
    writeLatch(addr, data);
    mappedAddr = 0;
    return true;
}

bool MapperMulticart::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = chrBanks ? chrBanks : 1u;
    mappedAddr = uint32_t(chrPage % count) * 0x2000u + addr;
    return true;
}
bool MapperMulticart::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (chrBanks != 0 || addr > 0x1FFFu) return false;
    mappedAddr = addr;
    return true;
}

void MapperMulticart::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u16(out, prgPage[0]); write_u16(out, prgPage[1]); write_u16(out, chrPage);
    write_u8(out, regs[0]); write_u8(out, regs[1]); write_u8(out, mirrorMode);
    write_bool(out, resetToggle); write_bool(out, resetSeen); write_bool(out, suppressInitialReset);
}
bool MapperMulticart::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u16(p,end,prgPage[0]) && read_u16(p,end,prgPage[1]) && read_u16(p,end,chrPage) &&
           read_u8(p,end,regs[0]) && read_u8(p,end,regs[1]) && read_u8(p,end,mirrorMode) &&
           read_bool(p,end,resetToggle) && read_bool(p,end,resetSeen) && read_bool(p,end,suppressInitialReset);
}
