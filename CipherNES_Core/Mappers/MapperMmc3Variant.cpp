#include "MapperMmc3Variant.h"
#include "../state_io.h"
#include <algorithm>
#include <cstring>

MapperMmc3Variant::MapperMmc3Variant(uint16_t p, uint16_t c, uint16_t m)
    : Mapper(p, c), mapperNumber(m), core(uint8_t(p), uint8_t(c)) {
    switch (mapperNumber) {
        case 74:
            chrRamFirstPage = 0x08;
            chrRamLastPage = 0x09;
            chrRamPageCount = 2;
            break;
        case 119:
            chrRamFirstPage = 0x40;
            chrRamLastPage = 0x7F;
            chrRamPageCount = 8;
            break;
        case 195:
            chrRamFirstPage = 0x00;
            chrRamLastPage = 0x03;
            chrRamPageCount = 4;
            break;
        case 199:
            chrRamFirstPage = 0x00;
            chrRamLastPage = 0x07;
            chrRamPageCount = 8;
            break;
        default:
            break;
    }
    if (mapperNumber == 12 || mapperNumber == 114) core.setMmc3AlternateIrqRevision(true);
    reset(true);
}

uint32_t MapperMmc3Variant::capabilities() const {
    uint32_t c = MAPPER_CAP_IRQ | MAPPER_CAP_PPU_ADDRESS_BUS;
    if (mapperNumber == 115 || mapperNumber == 187) c |= MAPPER_CAP_CPU_DIRECT;
    if (mapperNumber == 254) c |= MAPPER_CAP_CPU_READ_FILTER;
    if (mapperNumber == 118) c |= MAPPER_CAP_NAMETABLE;
    else c |= MAPPER_CAP_MIRRORING;
    return c;
}

void MapperMmc3Variant::reset() { reset(true); }

void MapperMmc3Variant::reset(bool hardReset) {
    if (mapperNumber == 128u) {
        if (hardReset) resetCounter = 0;
        else resetCounter = uint8_t((resetCounter + 1u) & 3u);
    }
    core.reset();
    if (mapperNumber == 12 || mapperNumber == 114) core.setMmc3AlternateIrqRevision(true);
    bankSelect = prgMode = chrMode = 0;
    std::memset(bankRegs, 0, sizeof(bankRegs));
    std::memset(exRegs, 0, sizeof(exRegs));
    exIndex = 0;
    std::memset(ntPages, 0, sizeof(ntPages));
    ntRam.fill(0);
    variantMirror = 0xFF;
    prgRamEnabled = true;
    prgRamWriteProtected = false;

    if (mapperNumber == 199) {
        exRegs[0] = 0xFE;
        exRegs[1] = 0xFF;
        exRegs[2] = 0x01;
        exRegs[3] = 0x03;
    }

    if (mapperNumber == 45 && chrBanks == 0) {
        bankRegs[0] = 0;
        bankRegs[1] = 2;
        bankRegs[2] = 4;
        bankRegs[3] = 5;
        bankRegs[4] = 6;
        bankRegs[5] = 7;
    }
    if (mapperNumber == 45) exRegs[2] = 0x0F;
}

uint16_t MapperMmc3Variant::innerPrg(uint8_t slot) const {
    const uint16_t count = std::max<uint16_t>(1, uint16_t(prgBanks * 2u));
    const uint16_t last = uint16_t(count - 1u);
    const uint16_t second = count > 1 ? uint16_t(count - 2u) : 0u;
    if (!prgMode) {
        if (slot == 0) return bankRegs[6];
        if (slot == 1) return bankRegs[7];
        if (slot == 2) return second;
        return last;
    }
    if (slot == 0) return second;
    if (slot == 1) return bankRegs[7];
    if (slot == 2) return bankRegs[6];
    return last;
}

uint16_t MapperMmc3Variant::innerChr(uint8_t slot) const {
    const uint16_t r0 = uint16_t(bankRegs[0] & 0xFEu);
    const uint16_t r1 = uint16_t(bankRegs[1] & 0xFEu);
    uint16_t map[8];
    if (!chrMode) {
        map[0] = r0; map[1] = uint16_t(r0 + 1u);
        map[2] = r1; map[3] = uint16_t(r1 + 1u);
        map[4] = bankRegs[2]; map[5] = bankRegs[3];
        map[6] = bankRegs[4]; map[7] = bankRegs[5];
    } else {
        map[0] = bankRegs[2]; map[1] = bankRegs[3];
        map[2] = bankRegs[4]; map[3] = bankRegs[5];
        map[4] = r0; map[5] = uint16_t(r0 + 1u);
        map[6] = r1; map[7] = uint16_t(r1 + 1u);
    }
    return map[slot & 7u];
}

uint32_t MapperMmc3Variant::transformPrgPage(uint32_t page, uint8_t slot) const {
    switch (mapperNumber) {
        case 128:
            page = (page & 0x0Fu) | (uint32_t(resetCounter & 3u) << 4);
            break;
        case 44: {
            uint32_t block = exRegs[0] & 0x07u;
            if (block == 7u) block = 6u;
            page &= block <= 5u ? 0x0Fu : 0x1Fu;
            page |= block * 0x10u;
            break;
        }
        case 45:
            page = (page & (0x3Fu ^ (exRegs[3] & 0x3Fu))) | exRegs[1];
            break;
        case 47:
            page = (page & 0x0Fu) | ((exRegs[0] & 1u) << 4);
            break;
        case 49:
            if (exRegs[2] & 1u) {
                page = (page & 0x0Fu) | ((exRegs[0] & 3u) << 4);
            } else {
                page = uint32_t(exRegs[1] & 3u) * 4u + slot;
            }
            break;
        case 52:
            if (exRegs[0] & 0x08u) {
                page = (page & 0x0Fu) | (uint32_t(exRegs[0] & 0x07u) << 4);
            } else {
                page = (page & 0x1Fu) | (uint32_t(exRegs[0] & 0x06u) << 4);
            }
            break;
        case 114:
            if (exRegs[0] & 0x80u) {
                const uint32_t base = uint32_t(exRegs[0] & 0x0Fu) << 1;
                page = base + (slot & 1u);
            }
            break;
        case 115:
            if (exRegs[0] & 0x80u) {
                if (exRegs[0] & 0x20u) {
                    const uint32_t base = uint32_t((exRegs[0] & 0x0Fu) >> 1) << 2;
                    page = base + slot;
                } else {
                    const uint32_t base = uint32_t(exRegs[0] & 0x0Fu) << 1;
                    page = base + (slot & 1u);
                }
            }
            break;
        case 123:
            if (exRegs[0] & 0x40u) {
                const uint32_t bank = (exRegs[0] & 0x05u)
                    | ((exRegs[0] & 0x08u) >> 2)
                    | ((exRegs[0] & 0x20u) >> 2);
                if (exRegs[0] & 0x02u) {
                    const uint32_t base = (bank & 0xFEu) << 1;
                    page = base + slot;
                } else {
                    const uint32_t base = bank << 1;
                    page = base + (slot & 1u);
                }
            }
            break;
        case 187:
            if (!(exRegs[0] & 0x80u)) {
                page &= 0x3Fu;
            } else {
                const uint32_t exPage = exRegs[0] & 0x1Fu;
                if (exRegs[0] & 0x20u) {
                    page = (exRegs[0] & 0x40u)
                        ? ((exPage & 0xFCu) + slot)
                        : (((exPage & 0xFEu) << 1) + slot);
                } else {
                    page = (exPage << 1) + (slot & 1u);
                }
            }
            break;
        case 189:
            page = uint32_t(((exRegs[0] | (exRegs[0] >> 4)) & 0x07u) * 4u + slot);
            break;
        case 196:
            if (exRegs[0]) page = uint32_t(exRegs[1]) * 4u + slot;
            break;
        case 199:
            if (slot == 2u) page = exRegs[0];
            else if (slot == 3u) page = exRegs[1];
            break;
        case 205: {
            const uint32_t block = exRegs[0] & 3u;
            page &= block <= 1u ? 0x1Fu : 0x0Fu;
            page |= block * 0x10u;
            break;
        }
        case 224: {
            const uint32_t outer = uint32_t(exRegs[0] & 1u) << 6;
            if (!prgMode) {
                if (slot == 0) page = bankRegs[6] & 0x3Fu;
                else if (slot == 1) page = bankRegs[7] & 0x3Fu;
                else if (slot == 2) page = 0x3Eu;
                else page = 0x3Fu;
            } else {
                if (slot == 0) page = 0x3Eu;
                else if (slot == 1) page = bankRegs[6] & 0x3Fu;
                else if (slot == 2) page = bankRegs[7] & 0x3Fu;
                else page = 0x3Fu;
            }
            page |= outer;
            break;
        }
        case 245: {
            const uint32_t outer = (bankRegs[0] & 2u) ? 0x40u : 0u;
            page = (page & 0x3Fu) | outer;
            break;
        }
        case 249:
            if (exRegs[0] & 2u) {
                if (page < 0x20u) {
                    page = (page & 1u) | ((page >> 3) & 2u) | ((page >> 1) & 4u) | ((page << 2) & 0x18u);
                } else {
                    page -= 0x20u;
                    page = (page & 3u) | ((page >> 1) & 4u) | ((page >> 4) & 8u)
                         | ((page >> 2) & 0x10u) | ((page << 3) & 0x20u)
                         | ((page << 2) & 0xC0u);
                }
            }
            break;
        default:
            break;
    }
    return page;
}

uint32_t MapperMmc3Variant::transformChrPage(uint32_t page, uint8_t slot) const {
    switch (mapperNumber) {
        case 128:
            page = (page & 0x7Fu) | (uint32_t(resetCounter & 3u) << 7);
            break;
        case 12:
            if (slot < 4u && (exRegs[0] & 0x01u)) page |= 0x100u;
            else if (slot >= 4u && (exRegs[0] & 0x10u)) page |= 0x100u;
            break;
        case 44: {
            uint32_t block = exRegs[0] & 0x07u;
            if (block == 7u) block = 6u;
            page &= block <= 5u ? 0x7Fu : 0xFFu;
            page |= block * 0x80u;
            break;
        }
        case 45:
            if (chrBanks) {
                const uint8_t n = uint8_t(exRegs[2] & 0x0Fu);
                const uint32_t mask = uint32_t(0xFFu >> (0x0Fu - n));
                page = (page & mask) | exRegs[0] | (uint32_t(exRegs[2] & 0xF0u) << 4);
            }
            break;
        case 47:
            page = (page & 0x7Fu) | ((exRegs[0] & 1u) << 7);
            break;
        case 49:
            page = (page & 0x7Fu) | (uint32_t(exRegs[0] & 3u) << 7);
            break;
        case 52:
            if (exRegs[0] & 0x40u) {
                page = (page & 0x7Fu)
                     | (uint32_t((exRegs[0] & 0x04u) | ((exRegs[0] >> 4) & 0x03u)) << 7);
            } else {
                page = (page & 0xFFu)
                     | (uint32_t((exRegs[0] & 0x04u) | ((exRegs[0] >> 4) & 0x02u)) << 7);
            }
            break;
        case 115:
            page |= uint32_t(exRegs[1] & 1u) << 8;
            break;
        case 187:
            if ((chrMode && slot >= 4u) || (!chrMode && slot < 4u)) page |= 0x100u;
            break;
        case 199:
            if (slot == 0u) page = bankRegs[0];
            else if (slot == 1u) page = exRegs[2];
            else if (slot == 2u) page = bankRegs[1];
            else if (slot == 3u) page = exRegs[3];
            break;
        case 205: {
            const uint32_t block = exRegs[0] & 3u;
            if (block >= 2u) {
                page &= 0x7Fu;
                page |= 0x100u;
            }
            if (block == 1u || block == 3u) page |= 0x80u;
            break;
        }
        case 245:
            if (chrBanks == 0) {
                page = uint32_t((chrMode ? (slot < 4u ? 4u : 0u) : (slot < 4u ? 0u : 4u)) + (page & 3u));
            }
            break;
        case 249:
            if (exRegs[0] & 2u) {
                page = (page & 3u) | ((page >> 1) & 4u) | ((page >> 4) & 8u)
                     | ((page >> 2) & 0x10u) | ((page << 3) & 0x20u)
                     | ((page << 2) & 0xC0u);
            }
            break;
        default:
            break;
    }
    return page;
}

bool MapperMmc3Variant::isMixedChrRamPage(uint32_t page) const {
    return chrRamPageCount != 0 && page >= chrRamFirstPage && page <= chrRamLastPage;
}

uint32_t MapperMmc3Variant::mapChrPage(uint32_t page, uint32_t offset) const {
    if (isMixedChrRamPage(page)) {
        const uint32_t ramPage = (page - chrRamFirstPage) % chrRamPageCount;
        return Mapper::CHR_RAM_FLAG | (ramPage * 0x400u + offset);
    }
    const uint32_t count = std::max<uint32_t>(1, chrBanks ? uint32_t(chrBanks) * 8u : 8u);
    return (page % count) * 0x400u + offset;
}

void MapperMmc3Variant::observeMmc3Write(uint16_t a, uint8_t d) {
    if ((a & 0xE001u) == 0x8000u) {
        bankSelect = d;
        prgMode = uint8_t((d >> 6) & 1u);
        chrMode = uint8_t((d >> 7) & 1u);
    } else if ((a & 0xE001u) == 0x8001u) {
        const uint8_t reg = uint8_t(bankSelect & 7u);
        if (mapperNumber == 118) {
            const uint8_t nt = uint8_t(d >> 7);
            if (!chrMode) {
                if (reg == 0) ntPages[0] = ntPages[1] = nt;
                else if (reg == 1) ntPages[2] = ntPages[3] = nt;
            } else if (reg >= 2 && reg <= 5) {
                ntPages[reg - 2] = nt;
            }
        }
        bankRegs[reg] = d;
    }
}

bool MapperMmc3Variant::cpuReadDirect(uint16_t a, uint8_t& d) {
    if (mapperNumber == 115 && a >= 0x5000u && a <= 0x5FFFu) {
        d = exRegs[2];
        return true;
    }
    if (mapperNumber == 187 && a >= 0x5000u && a <= 0x5FFFu) {
        static const uint8_t security[4] = { 0x83, 0x83, 0x42, 0x00 };
        d = security[exRegs[1] & 3u];
        return true;
    }
    return false;
}

void MapperMmc3Variant::cpuFilterRead(uint16_t a, uint8_t& d) {
    if (mapperNumber == 254 && a >= 0x6000u && a <= 0x7FFFu && !exRegs[0]) {
        d ^= exRegs[1];
    }
}

bool MapperMmc3Variant::cpuMapRead(uint16_t a, uint32_t& m) {
    if (a < 0x8000u) return core.cpuMapRead(a, m);
    const uint8_t slot = uint8_t((a - 0x8000u) >> 13);
    uint32_t page = transformPrgPage(innerPrg(slot), slot);
    const uint32_t count = std::max<uint32_t>(1, uint32_t(prgBanks) * 2u);
    m = (page % count) * 0x2000u + (a & 0x1FFFu);
    return true;
}

bool MapperMmc3Variant::cpuMapWrite(uint16_t a, uint32_t& m, uint8_t d) {
    const auto writeCore = [&](uint16_t target, uint8_t value) -> bool {
        if (target >= 0x8000u) observeMmc3Write(target, value);
        return core.cpuMapWrite(target, m, value);
    };

    static const uint8_t security[8] = { 0, 3, 1, 5, 6, 7, 2, 4 };

    if (mapperNumber == 114) {
        if (a < 0x8000u) {
            if (a >= 0x5000u) { exRegs[0] = d; m = 0; return true; }
            return false;
        }
        switch (a & 0xE001u) {
            case 0x8001u: return writeCore(0xA000u, d);
            case 0xA000u:
                exRegs[1] = 1;
                return writeCore(0x8000u, uint8_t((d & 0xC0u) | security[d & 7u]));
            case 0xA001u: return writeCore(0xC000u, d);
            case 0xC000u:
                if (exRegs[1]) { exRegs[1] = 0; return writeCore(0x8001u, d); }
                m = 0; return true;
            case 0xC001u: return writeCore(0xC001u, d);
            case 0xE000u: return writeCore(0xE000u, d);
            case 0xE001u: return writeCore(0xE001u, d);
            default: m = 0; return true;
        }
    }

    if (mapperNumber == 115 && a >= 0x4100u && a < 0x8000u) {
        if (a == 0x5080u) exRegs[2] = d;
        else if (a & 1u) exRegs[1] = uint8_t(d & 1u);
        else exRegs[0] = d;
        m = 0;
        return true;
    }

    if (mapperNumber == 123) {
        if (a < 0x8000u && (a & 0x0800u)) {
            exRegs[a & 1u] = d;
            m = 0;
            return true;
        }
        if (a >= 0x8000u && a < 0xA000u) {
            const uint8_t value = (a & 1u) ? d : uint8_t((d & 0xC0u) | security[d & 7u]);
            return writeCore(a, value);
        }
    }

    if (mapperNumber == 187) {
        if (a == 0x5000u || a == 0x6000u) {
            exRegs[0] = d;
            m = 0;
            return true;
        }
        if (a == 0x8000u) exRegs[1] = 1;
        if (a == 0x8001u && exRegs[1] != 1u) { m = 0; return true; }
    }

    if (mapperNumber == 199 && a == 0x8001u && (bankSelect & 0x08u)) {
        exRegs[bankSelect & 0x03u] = d;
        m = 0;
        return true;
    }
    if (mapperNumber == 199 && (a & 0xE001u) == 0xA000u) {
        switch (d & 3u) {
            case 0: variantMirror = 1; break;
            case 1: variantMirror = 0; break;
            case 2: variantMirror = 3; break;
            default: variantMirror = 4; break;
        }
    }
    if (mapperNumber == 254) {
        if (a == 0x8000u) exRegs[0] = 0xFF;
        if (a == 0xA001u) {
            exRegs[1] = d;
            prgRamEnabled = (d & 0x80u) != 0;
            prgRamWriteProtected = (d & 0x40u) != 0;
        }
    }

    if (mapperNumber == 182 && a >= 0x8000u) {
        static const uint8_t security[8] = { 0, 3, 1, 5, 6, 7, 2, 4 };
        switch (a & 0xE001u) {
            case 0x8001u: return writeCore(0xA000u, d);
            case 0xA000u: return writeCore(0x8000u, uint8_t((d & 0xF8u) | security[d & 7u]));
            case 0xC000u: return writeCore(0x8001u, d);
            case 0xC001u:
                writeCore(0xC000u, d);
                return writeCore(0xC001u, d);
            case 0xE000u: return writeCore(0xE000u, d);
            case 0xE001u: return writeCore(0xE001u, d);
            default: m = 0; return true;
        }
    }
    if (mapperNumber == 250 && a >= 0x8000u) {
        const uint16_t target = uint16_t((a & 0xE000u) | ((a & 0x0400u) >> 10));
        return writeCore(target, uint8_t(a & 0x00FFu));
    }
    if (mapperNumber == 196 && a >= 0x8000u) {
        uint16_t target;
        if (a >= 0xC000u) {
            target = uint16_t((a & 0xFFFEu) | ((a >> 2) & 1u) | ((a >> 3) & 1u));
        } else {
            target = uint16_t((a & 0xFFFEu) | ((a >> 2) & 1u) | ((a >> 3) & 1u) | ((a >> 1) & 1u));
        }
        return writeCore(target, d);
    }
    if (mapperNumber == 189 && a >= 0x4120u && a <= 0x7FFFu) {
        exRegs[0] = d;
        m = 0;
        return true;
    }
    if (mapperNumber == 196 && a >= 0x6000u && a <= 0x6FFFu) {
        exRegs[0] = 1;
        exRegs[1] = uint8_t((d & 0x0Fu) | (d >> 4));
        m = 0;
        return true;
    }
    if (mapperNumber == 205 && a >= 0x6000u && a <= 0x7FFFu) {
        exRegs[0] = uint8_t(d & 3u);
        m = 0;
        return true;
    }
    if (mapperNumber == 224 && a == 0x5000u) {
        exRegs[0] = uint8_t((d >> 2) & 1u);
        m = 0;
        return true;
    }
    if (mapperNumber == 12 && a >= 0x4020u && a <= 0x5FFFu) {
        exRegs[0] = d;
        m = 0;
        return true;
    }
    if (mapperNumber == 249 && a == 0x5000u) {
        exRegs[0] = d;
        m = 0;
        return true;
    }
    if (mapperNumber == 45 && a >= 0x6000u && a <= 0x7FFFu && !(exRegs[3] & 0x40u)) {
        exRegs[exIndex] = d;
        exIndex = uint8_t((exIndex + 1u) & 3u);
        m = 0;
        return true;
    }
    if ((mapperNumber == 47 || mapperNumber == 49 || mapperNumber == 52)
        && a >= 0x6000u && a <= 0x7FFFu) {
        uint32_t ramMap = 0;
        const bool writable = core.cpuMapWrite(a, ramMap, d)
            && (ramMap & Mapper::PRG_RAM_FLAG) != 0;
        if (writable) {
            if (mapperNumber == 47) {
                exRegs[0] = uint8_t(d & 1u);
                m = 0;
            } else if (mapperNumber == 49) {
                exRegs[0] = uint8_t((d >> 6) & 3u);
                exRegs[1] = uint8_t((d >> 4) & 3u);
                exRegs[2] = uint8_t(d & 1u);
                m = 0;
            } else if ((exRegs[0] & 0x80u) == 0) {
                exRegs[0] = d;
                m = 0;
            } else {
                m = ramMap;
            }
        } else {
            m = 0;
        }
        return true;
    }
    if (mapperNumber == 44 && (a & 0xE001u) == 0xA001u) {
        exRegs[0] = uint8_t(d & 7u);
        if (exRegs[0] == 7u) exRegs[0] = 6u;
    }
    if (a >= 0x8000u) return writeCore(a, d);
    return core.cpuMapWrite(a, m, d);
}

bool MapperMmc3Variant::ppuMapRead(uint16_t a, uint32_t& m) {
    if (a > 0x1FFFu) return false;
    if (mapperNumber == 165) {
        const uint8_t half = uint8_t(a >> 12);
        const bool latch = (exRegs[0] & (1u << half)) != 0;
        const uint8_t reg = half == 0 ? (latch ? 1u : 0u) : (latch ? 4u : 2u);
        const uint8_t value = bankRegs[reg];
        if (value == 0) m = Mapper::CHR_RAM_FLAG | uint32_t(a & 0x0FFFu);
        else {
            const uint32_t count4k = std::max<uint32_t>(1, uint32_t(chrBanks) * 2u);
            m = (uint32_t(value >> 2) % count4k) * 0x1000u + (a & 0x0FFFu);
        }
        return true;
    }
    const uint8_t slot = uint8_t(a >> 10);
    const uint32_t page = transformChrPage(innerChr(slot), slot);
    m = mapChrPage(page, a & 0x3FFu);
    return true;
}

bool MapperMmc3Variant::ppuMapWrite(uint16_t a, uint32_t& m, uint8_t) {
    if (a > 0x1FFFu) return false;
    if (mapperNumber == 165) {
        const uint8_t half = uint8_t(a >> 12);
        const bool latch = (exRegs[0] & (1u << half)) != 0;
        const uint8_t reg = half == 0 ? (latch ? 1u : 0u) : (latch ? 4u : 2u);
        if (bankRegs[reg] != 0) return false;
        m = Mapper::CHR_RAM_FLAG | uint32_t(a & 0x0FFFu);
        return true;
    }
    const uint8_t slot = uint8_t(a >> 10);
    const uint32_t page = transformChrPage(innerChr(slot), slot);
    if (chrRamPageCount != 0) {
        if (!isMixedChrRamPage(page)) return false;
        m = mapChrPage(page, a & 0x3FFu);
        return true;
    }
    if (chrBanks != 0) return false;
    m = mapChrPage(page, a & 0x3FFu);
    return true;
}

void MapperMmc3Variant::ppuAddressBus(uint16_t a) {
    core.ppuAddressBus(a);
    if (mapperNumber != 165) return;

    if (exRegs[2]) {
        exRegs[0] = exRegs[1];
        exRegs[2] = 0;
    }
    const uint16_t key = uint16_t(a & 0x2FF8u);
    if (key == 0x0FD0u || key == 0x0FE8u) {
        const uint8_t half = uint8_t((a >> 12) & 1u);
        const uint8_t mask = uint8_t(1u << half);
        if (a & 0x08u) exRegs[1] |= mask;
        else exRegs[1] &= uint8_t(~mask);
        exRegs[2] = 1;
    }
}

bool MapperMmc3Variant::nametableRead(uint16_t a, uint8_t& d) {
    if (mapperNumber != 118) return false;
    const uint16_t f = uint16_t((a - 0x2000u) & 0x0FFFu);
    const uint8_t t = uint8_t(f >> 10);
    d = ntRam[uint16_t(ntPages[t] & 1u) * 0x400u + (f & 0x3FFu)];
    return true;
}

bool MapperMmc3Variant::nametableWrite(uint16_t a, uint8_t d) {
    if (mapperNumber != 118) return false;
    const uint16_t f = uint16_t((a - 0x2000u) & 0x0FFFu);
    const uint8_t t = uint8_t(f >> 10);
    ntRam[uint16_t(ntPages[t] & 1u) * 0x400u + (f & 0x3FFu)] = d;
    return true;
}

uint8_t MapperMmc3Variant::mirroringOverride() const {
    if (mapperNumber == 118) return 0xFF;
    if (mapperNumber == 199) return variantMirror;
    return core.mirroringOverride();
}

void MapperMmc3Variant::saveState(std::vector<uint8_t>& o) const {
    using namespace ciphernes_state;
    std::vector<uint8_t> c;
    core.saveState(c);
    write_vec(o, c);
    write_u8(o, bankSelect);
    write_u8(o, prgMode);
    write_u8(o, chrMode);
    write_bytes(o, bankRegs, sizeof(bankRegs));
    write_bytes(o, exRegs, sizeof(exRegs));
    write_u8(o, exIndex);
    write_bytes(o, ntPages, sizeof(ntPages));
    write_u8(o, variantMirror);
    write_bool(o, prgRamEnabled);
    write_bool(o, prgRamWriteProtected);
    write_bytes(o, ntRam.data(), ntRam.size());
    if (mapperNumber == 128u) write_u8(o, resetCounter);
}

bool MapperMmc3Variant::loadState(const uint8_t*& p, const uint8_t* e) {
    using namespace ciphernes_state;
    std::vector<uint8_t> c;
    if (!read_vec(p, e, c)) return false;
    const uint8_t* q = c.data();
    if (!core.loadState(q, q + c.size())) return false;
    if (mapperNumber == 12 || mapperNumber == 114) core.setMmc3AlternateIrqRevision(true);
    if (!read_u8(p, e, bankSelect)
        || !read_u8(p, e, prgMode)
        || !read_u8(p, e, chrMode)
        || !read_bytes(p, e, bankRegs, sizeof(bankRegs))
        || !read_bytes(p, e, exRegs, sizeof(exRegs))
        || !read_u8(p, e, exIndex)
        || !read_bytes(p, e, ntPages, sizeof(ntPages))
        || !read_u8(p, e, variantMirror)
        || !read_bool(p, e, prgRamEnabled)
        || !read_bool(p, e, prgRamWriteProtected)
        || !read_bytes(p, e, ntRam.data(), ntRam.size())) return false;
    return mapperNumber != 128u || read_u8(p, e, resetCounter);
}
