#include "MapperCommonBoards.h"
#include "../state_io.h"

#include <algorithm>
#include <cstring>

MapperCommonBoards::MapperCommonBoards(uint16_t p, uint16_t c, uint16_t m)
    : Mapper(p, c), mapperNumber(m) { reset(); }

uint32_t MapperCommonBoards::capabilities() const {
    uint32_t caps = 0;
    switch (mapperNumber) {
        case 41: case 57: case 89: case 112: case 200: case 202:
        case 204: case 242: case 243:
            caps |= MAPPER_CAP_MIRRORING;
            break;
        default: break;
    }
    if (mapperNumber == 41) caps |= MAPPER_CAP_BUS_CONFLICT;
    if (mapperNumber == 243) caps |= MAPPER_CAP_CPU_DIRECT | MAPPER_CAP_NAMETABLE;
    return caps;
}

void MapperCommonBoards::setPrg32(uint16_t bank32) {
    const uint16_t base = uint16_t(bank32 * 4u);
    for (int i = 0; i < 4; ++i) prg8[i] = uint16_t(base + i);
}
void MapperCommonBoards::setPrg16(int slot16, uint16_t bank16) {
    const uint16_t base = uint16_t(bank16 * 2u);
    prg8[slot16 * 2] = base;
    prg8[slot16 * 2 + 1] = uint16_t(base + 1u);
}
void MapperCommonBoards::setChr8(uint16_t bank8) {
    const uint16_t base = uint16_t(bank8 * 8u);
    for (int i = 0; i < 8; ++i) chr1[i] = uint16_t(base + i);
}

void MapperCommonBoards::reset() {
    std::memset(regs, 0, sizeof(regs));
    currentReg = outerChr = prgBank41 = chrBank41 = 0;
    ntRam.fill(0);
    mirrorMode = 0xFF;
    setPrg32(0);
    setChr8(0);

    const uint16_t count8 = std::max<uint16_t>(1u, uint16_t(prgBanks * 2u));
    switch (mapperNumber) {
        case 41:
            mirrorMode = 1;
            break;
        case 57:
            mirrorMode = 1;
            break;
        case 89:
            setPrg16(1, prgBanks ? uint16_t(prgBanks - 1u) : 0u);
            mirrorMode = 3;
            break;
        case 112:
            prg8[2] = count8 > 1u ? uint16_t(count8 - 2u) : 0u;
            prg8[3] = uint16_t(count8 - 1u);
            mirrorMode = 1;
            update112();
            break;
        case 200:
            setPrg16(0, 0); setPrg16(1, 0); mirrorMode = 0;
            break;
        case 201:
            break;
        case 202:
            setPrg16(0, 0); setPrg16(1, 0); mirrorMode = 1;
            break;
        case 203:
            setPrg16(0, 0); setPrg16(1, 0);
            break;
        case 204:
            writeRegister(0x8000, 0);
            break;
        case 242:
            setPrg32(0); mirrorMode = 1;
            break;
        case 243:
            update243();
            break;
        case 246:
            prg8[0] = prg8[1] = prg8[2] = 0;
            prg8[3] = 0xFFu;
            break;
        default: break;
    }
}

void MapperCommonBoards::update112() {
    prg8[0] = regs[0];
    prg8[1] = regs[1];
    chr1[0] = uint16_t(regs[2] & 0xFEu);
    chr1[1] = uint16_t(chr1[0] + 1u);
    chr1[2] = uint16_t(regs[3] & 0xFEu);
    chr1[3] = uint16_t(chr1[2] + 1u);
    chr1[4] = uint16_t(regs[4] | ((outerChr & 0x10u) << 4));
    chr1[5] = uint16_t(regs[5] | ((outerChr & 0x20u) << 3));
    chr1[6] = uint16_t(regs[6] | ((outerChr & 0x40u) << 2));
    chr1[7] = uint16_t(regs[7] | ((outerChr & 0x80u) << 1));
}

void MapperCommonBoards::update243() {
    const uint16_t prg32 = uint16_t(regs[5] & 0x03u);
    const uint16_t chr8 = uint16_t((regs[2] & 1u) | ((regs[4] & 1u) << 1)
                                | ((regs[6] & 0x03u) << 2));
    setPrg32(prg32);
    setChr8(chr8);
    switch ((regs[7] >> 1) & 3u) {
        case 0: mirrorMode = 0xFF; break; // S0-S0-S0-S1 is not representable by CIRAM override.
        case 1: mirrorMode = 0; break;
        case 2: mirrorMode = 1; break;
        default: mirrorMode = 4; break;
    }
}

bool MapperCommonBoards::cpuReadDirect(uint16_t addr, uint8_t& data) {
    if (mapperNumber != 243) return false;
    if ((addr & 0xC101u) == 0x4101u) {
        data = regs[currentReg & 7u];
        return true;
    }
    return false;
}

bool MapperCommonBoards::cpuWriteDirect(uint16_t addr, uint8_t data) {
    if (mapperNumber != 243) return false;
    switch (addr & 0xC101u) {
        case 0x4100u: currentReg = uint8_t(data & 7u); return true;
        case 0x4101u:
            regs[currentReg & 7u] = uint8_t(data & 7u);
            update243();
            return true;
        default: return false;
    }
}

void MapperCommonBoards::writeRegister(uint16_t addr, uint8_t data) {
    switch (mapperNumber) {
        case 41:
            if (addr <= 0x67FFu) {
                prgBank41 = uint8_t(addr & 7u);
                chrBank41 = uint8_t((chrBank41 & 3u) | ((addr >> 1) & 0x0Cu));
                setPrg32(prgBank41);
                setChr8(chrBank41);
                mirrorMode = (addr & 0x20u) ? 0u : 1u;
            } else if (prgBank41 >= 4u) {
                chrBank41 = uint8_t((chrBank41 & 0x0Cu) | (data & 3u));
                setChr8(chrBank41);
            }
            break;
        case 57:
            regs[(addr & 0x0800u) ? 1 : 0] = data;
            mirrorMode = (regs[1] & 0x08u) ? 0u : 1u;
            setChr8(uint16_t(((regs[0] & 0x40u) >> 3) | ((regs[0] | regs[1]) & 7u)));
            if (regs[1] & 0x10u) {
                const uint16_t page = uint16_t((regs[1] >> 5) & 6u);
                setPrg16(0, page); setPrg16(1, uint16_t(page + 1u));
            } else {
                const uint16_t page = uint16_t((regs[1] >> 5) & 7u);
                setPrg16(0, page); setPrg16(1, page);
            }
            break;
        case 89:
            setPrg16(0, uint16_t((data >> 4) & 7u));
            setChr8(uint16_t((data & 7u) | ((data & 0x80u) >> 4)));
            mirrorMode = (data & 8u) ? 4u : 3u;
            break;
        case 112:
            switch (addr & 0xE001u) {
                case 0x8000u: currentReg = uint8_t(data & 7u); break;
                case 0xA000u: regs[currentReg] = data; break;
                case 0xC000u: outerChr = data; break;
                case 0xE000u: mirrorMode = (data & 1u) ? 0u : 1u; break;
                default: break;
            }
            update112();
            break;
        case 200: {
            const uint16_t bank = uint16_t(addr & 7u);
            setPrg16(0, bank); setPrg16(1, bank); setChr8(bank);
            mirrorMode = (addr & 8u) ? 1u : 0u;
            break;
        }
        case 201:
            setPrg32(uint16_t(addr & 3u)); setChr8(uint16_t(addr & 7u));
            break;
        case 202: {
            const uint16_t bank = uint16_t((addr >> 1) & 7u);
            setChr8(bank);
            if ((addr & 9u) == 9u) { setPrg16(0, bank); setPrg16(1, uint16_t(bank + 1u)); }
            else { setPrg16(0, bank); setPrg16(1, bank); }
            mirrorMode = (addr & 1u) ? 0u : 1u;
            break;
        }
        case 203:
            setPrg16(0, uint16_t(data >> 2)); setPrg16(1, uint16_t(data >> 2));
            setChr8(uint16_t(data & 3u));
            break;
        case 204: {
            const uint16_t mask = uint16_t(addr & 6u);
            const uint16_t page0 = uint16_t(mask + (mask == 6u ? 0u : (addr & 1u)));
            const uint16_t page1 = uint16_t(mask + (mask == 6u ? 1u : (addr & 1u)));
            setPrg16(0, page0); setPrg16(1, page1); setChr8(page0);
            mirrorMode = (addr & 0x10u) ? 0u : 1u;
            break;
        }
        case 242:
            setPrg32(uint16_t((addr >> 3) & 0x0Fu));
            mirrorMode = (addr & 2u) ? 0u : 1u;
            break;
        case 246:
            if ((addr & 7u) <= 3u) prg8[addr & 3u] = data;
            else {
                const uint8_t slot2k = uint8_t(addr & 3u);
                const uint16_t base = uint16_t(data * 2u);
                chr1[slot2k * 2u] = base;
                chr1[slot2k * 2u + 1u] = uint16_t(base + 1u);
            }
            break;
        default: break;
    }
}

bool MapperCommonBoards::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (mapperNumber == 246 && addr >= 0x6000u && addr <= 0x7FFFu) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000u) return false;
    const uint16_t count = std::max<uint16_t>(1u, uint16_t(prgBanks * 2u));
    const uint8_t slot = uint8_t((addr - 0x8000u) >> 13);
    mappedAddr = uint32_t(prg8[slot] % count) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

bool MapperCommonBoards::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (mapperNumber == 41) {
        if (!((addr >= 0x6000u && addr <= 0x67FFu) || addr >= 0x8000u)) return false;
    } else if (mapperNumber == 246) {
        if (addr >= 0x6000u && addr <= 0x67FFu) { writeRegister(addr, data); mappedAddr = 0; return true; }
        if (addr >= 0x6800u && addr <= 0x7FFFu) {
            mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu); return true;
        }
        return false;
    } else if (mapperNumber == 243) {
        return false; // handled by cpuWriteDirect
    } else if (addr < 0x8000u) {
        return false;
    }
    writeRegister(addr, data);
    mappedAddr = 0;
    return true;
}

bool MapperCommonBoards::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFFu) return false;
    const uint16_t count = std::max<uint16_t>(1u, uint16_t(chrBanks ? chrBanks * 8u : 8u));
    const uint8_t slot = uint8_t(addr >> 10);
    mappedAddr = uint32_t(chr1[slot] % count) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}
bool MapperCommonBoards::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    return chrBanks == 0 && ppuMapRead(addr, mappedAddr);
}

bool MapperCommonBoards::nametableRead(uint16_t addr, uint8_t& data) {
    if (mapperNumber != 243) return false;
    const uint16_t folded = uint16_t((addr - 0x2000u) & 0x0FFFu);
    const uint8_t table = uint8_t(folded >> 10);
    uint8_t page = 0;
    switch ((regs[7] >> 1) & 3u) {
        case 0: page = table == 3u ? 1u : 0u; break;
        case 1: page = uint8_t((table >> 1) & 1u); break;
        case 2: page = uint8_t(table & 1u); break;
        default: page = 1u; break;
    }
    data = ntRam[uint16_t(page) * 0x400u + (folded & 0x03FFu)];
    return true;
}
bool MapperCommonBoards::nametableWrite(uint16_t addr, uint8_t data) {
    if (mapperNumber != 243) return false;
    const uint16_t folded = uint16_t((addr - 0x2000u) & 0x0FFFu);
    const uint8_t table = uint8_t(folded >> 10);
    uint8_t page = 0;
    switch ((regs[7] >> 1) & 3u) {
        case 0: page = table == 3u ? 1u : 0u; break;
        case 1: page = uint8_t((table >> 1) & 1u); break;
        case 2: page = uint8_t(table & 1u); break;
        default: page = 1u; break;
    }
    ntRam[uint16_t(page) * 0x400u + (folded & 0x03FFu)] = data;
    return true;
}

void MapperCommonBoards::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    for (uint16_t v : prg8) write_u16(out, v);
    for (uint16_t v : chr1) write_u16(out, v);
    write_u8(out, mirrorMode); write_bytes(out, regs, sizeof(regs));
    write_u8(out, currentReg); write_u8(out, outerChr);
    write_u8(out, prgBank41); write_u8(out, chrBank41); write_bytes(out,ntRam.data(),ntRam.size());
}
bool MapperCommonBoards::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    for (uint16_t& v : prg8) if (!read_u16(p,end,v)) return false;
    for (uint16_t& v : chr1) if (!read_u16(p,end,v)) return false;
    return read_u8(p,end,mirrorMode) && read_bytes(p,end,regs,sizeof(regs)) &&
           read_u8(p,end,currentReg) && read_u8(p,end,outerChr) &&
           read_u8(p,end,prgBank41) && read_u8(p,end,chrBank41) && read_bytes(p,end,ntRam.data(),ntRam.size());
}
