#include "Mapper111.h"
#include "../state_io.h"

#include <algorithm>

Mapper111::Mapper111(uint16_t p, uint16_t c)
    : Mapper(p,c), legacyMode(c != 0u) { reset(); }

uint32_t Mapper111::capabilities() const {
    return legacyMode ? MAPPER_CAP_MIRRORING : MAPPER_CAP_NAMETABLE;
}

void Mapper111::reset() {
    control = 0x1Cu;
    chr0 = chr1 = prg = 0;
    gtReg = 0;
    gtNametable.fill(0);
    updateLegacyMirror();
}

void Mapper111::updateLegacyMirror() {
    switch (control & 3u) {
        case 0: mirrorMode = 3; break;
        case 1: mirrorMode = 4; break;
        case 2: mirrorMode = 1; break;
        default: mirrorMode = 0; break;
    }
}

uint32_t Mapper111::legacyPrgOffset(uint16_t addr) const {
    const uint32_t count16 = std::max<uint32_t>(1u, prgBanks);
    const uint8_t mode = uint8_t((control >> 2) & 3u);
    uint32_t bank = 0;
    if (mode <= 1u) {
        const uint32_t base = uint32_t(prg & 0xFEu);
        bank = base + ((addr >= 0xC000u) ? 1u : 0u);
    } else if (mode == 2u) {
        bank = (addr < 0xC000u) ? 0u : uint32_t(prg);
    } else {
        bank = (addr < 0xC000u) ? uint32_t(prg) : (count16 - 1u);
    }
    return (bank % count16) * 0x4000u + uint32_t(addr & 0x3FFFu);
}

uint32_t Mapper111::legacyChrOffset(uint16_t addr) const {
    const uint32_t count4 = std::max<uint32_t>(1u, uint32_t(chrBanks) * 2u);
    uint32_t bank4;
    if (control & 0x10u) {
        bank4 = (addr < 0x1000u) ? uint32_t(chr0) : uint32_t(chr1);
    } else {
        bank4 = uint32_t(chr0 & 0xFEu) + ((addr >= 0x1000u) ? 1u : 0u);
    }
    return (bank4 % count4) * 0x1000u + uint32_t(addr & 0x0FFFu);
}

bool Mapper111::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000u) return false;
    if (legacyMode) {
        mappedAddr = legacyPrgOffset(addr);
    } else {
        const uint32_t count32 = std::max<uint32_t>(1u, (uint32_t(prgBanks) + 1u) / 2u);
        mappedAddr = (uint32_t(gtReg & 0x0Fu) % count32) * 0x8000u + uint32_t(addr & 0x7FFFu);
    }
    return true;
}

bool Mapper111::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;
    if (legacyMode) {
        if (addr < 0x8000u) return false;
        switch ((addr >> 13) & 3u) {
            case 0: control = data; updateLegacyMirror(); break;
            case 1: chr0 = data; break;
            case 2: chr1 = data; break;
            case 3: prg = data; break;
        }
        return true;
    }

    if (addr >= 0x5000u && addr <= 0x5FFFu) {
        gtReg = data;
        return true;
    }
    if (addr >= 0x8000u) {
        // GTROM PRG is flash-backed. Ordinary games only bank through $5000;
        // flash command writes are accepted here so they never leak to the bus.
        return true;
    }
    return false;
}

bool Mapper111::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    addr &= 0x3FFFu;
    if (addr > 0x1FFFu) return false;
    if (legacyMode) {
        mappedAddr = legacyChrOffset(addr);
    } else {
        mappedAddr = CHR_RAM_FLAG | (uint32_t((gtReg >> 4) & 1u) * 0x2000u + addr);
    }
    return true;
}

bool Mapper111::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t) {
    addr &= 0x3FFFu;
    if (addr > 0x1FFFu || legacyMode) return false;
    mappedAddr = CHR_RAM_FLAG | (uint32_t((gtReg >> 4) & 1u) * 0x2000u + addr);
    return true;
}

bool Mapper111::nametableRead(uint16_t addr, uint8_t& data) {
    if (legacyMode) return false;
    const uint16_t off = uint16_t((addr - 0x2000u) & 0x1FFFu);
    const uint16_t page = uint16_t(((gtReg & 0x20u) ? 8u : 0u) + (off >> 10));
    data = gtNametable[uint16_t(page * 0x400u + (off & 0x3FFu))];
    return true;
}

bool Mapper111::nametableWrite(uint16_t addr, uint8_t data) {
    if (legacyMode) return false;
    const uint16_t off = uint16_t((addr - 0x2000u) & 0x1FFFu);
    const uint16_t page = uint16_t(((gtReg & 0x20u) ? 8u : 0u) + (off >> 10));
    gtNametable[uint16_t(page * 0x400u + (off & 0x3FFu))] = data;
    return true;
}

void Mapper111::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, control); write_u8(out, chr0); write_u8(out, chr1); write_u8(out, prg);
    write_u8(out, mirrorMode); write_u8(out, gtReg);
    write_bytes(out, gtNametable.data(), gtNametable.size());
}

bool Mapper111::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p,end,control) || !read_u8(p,end,chr0) || !read_u8(p,end,chr1) ||
        !read_u8(p,end,prg) || !read_u8(p,end,mirrorMode) || !read_u8(p,end,gtReg) ||
        !read_bytes(p,end,gtNametable.data(),gtNametable.size())) return false;
    updateLegacyMirror();
    return true;
}
