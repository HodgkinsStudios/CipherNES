#include "MapperJalecoSimple.h"

#include "../state_io.h"

MapperJalecoSimple::MapperJalecoSimple(uint16_t prgBanks,
                                       uint16_t chrBanks,
                                       uint16_t mapperNumber)
    : Mapper(prgBanks, chrBanks), mapperNumber(mapperNumber) {
    reset();
}

void MapperJalecoSimple::reset() {
    prg16[0] = 0;
    prg16[1] = prgBanks ? uint16_t(prgBanks - 1) : 0;
    chr8 = 0;
    prgFlag = false;
    chrFlag = false;
}

bool MapperJalecoSimple::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;

    const uint16_t count = prgBanks ? prgBanks : 1;
    const uint8_t slot = uint8_t((addr - 0x8000) >> 14);
    mappedAddr = uint32_t(prg16[slot] % count) * 0x4000u + (addr & 0x3FFFu);
    return true;
}

bool MapperJalecoSimple::cpuMapWrite(uint16_t addr,
                                     uint32_t& mappedAddr,
                                     uint8_t data) {
    if (mapperNumber == 86) {
        if (addr >= 0x6000 && addr <= 0x6FFF) {
            const uint16_t page32 = uint16_t((data & 0x30) >> 4);
            prg16[0] = uint16_t(page32 * 2);
            prg16[1] = uint16_t(page32 * 2 + 1);
            chr8 = uint16_t((data & 0x03) | ((data >> 4) & 0x04));
            mappedAddr = 0;
            return true;
        }
        if (addr >= 0x7000 && addr <= 0x7FFF) {
            // Optional sample playback register; consume the write so it does
            // not fall through to ordinary PRG-RAM handling.
            mappedAddr = 0;
            return true;
        }
        return false;
    }

    if (mapperNumber == 140) {
        if (addr < 0x6000 || addr > 0x7FFF) return false;
        const uint16_t page32 = uint16_t((data >> 4) & 0x03);
        prg16[0] = uint16_t(page32 * 2);
        prg16[1] = uint16_t(page32 * 2 + 1);
        chr8 = uint16_t(data & 0x0F);
        mappedAddr = 0;
        return true;
    }

    if (addr < 0x8000) return false;

    if (!prgFlag && (data & 0x80)) {
        if (mapperNumber == 92) {
            prg16[1] = uint16_t(data & 0x0F);
        } else {
            prg16[0] = uint16_t(data & 0x07);
        }
    }
    if (!chrFlag && (data & 0x40)) {
        chr8 = uint16_t(data & 0x0F);
    }

    prgFlag = (data & 0x80) != 0;
    chrFlag = (data & 0x40) != 0;
    mappedAddr = 0;
    return true;
}

bool MapperJalecoSimple::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    const uint16_t count = chrBanks ? chrBanks : 1;
    mappedAddr = uint32_t(chr8 % count) * 0x2000u + addr;
    return true;
}

bool MapperJalecoSimple::ppuMapWrite(uint16_t addr,
                                     uint32_t& mappedAddr,
                                     uint8_t /*data*/) {
    return chrBanks == 0 && ppuMapRead(addr, mappedAddr);
}

void MapperJalecoSimple::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u16(out, prg16[0]);
    write_u16(out, prg16[1]);
    write_u16(out, chr8);
    write_bool(out, prgFlag);
    write_bool(out, chrFlag);
}

bool MapperJalecoSimple::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u16(p, end, prg16[0])
        && read_u16(p, end, prg16[1])
        && read_u16(p, end, chr8)
        && read_bool(p, end, prgFlag)
        && read_bool(p, end, chrFlag);
}
