#include "Mapper034.h"

#include "../state_io.h"

void Mapper034::reset() {
    prgBankSelect = 0;
    chrBank0 = 0;
    chrBank1 = 1;
}

bool Mapper034::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000) {
        if (prgBanks <= 1) {
            mappedAddr = uint32_t(addr & 0x3FFF);
            return true;
        }
        const uint8_t banks32k = prgBanks > 1 ? uint8_t(prgBanks / 2) : 1;
        const uint8_t bank = banks32k ? uint8_t(prgBankSelect % banks32k) : 0;
        mappedAddr = uint32_t(bank) * 0x8000 + uint32_t(addr & 0x7FFF);
        return true;
    }

    return false;
}

bool Mapper034::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr == 0x7FFD) {
        const uint8_t banks32k = prgBanks > 1 ? uint8_t(prgBanks / 2) : 1;
        prgBankSelect = banks32k ? uint8_t(data % banks32k) : 0;
        mappedAddr = 0;
        return true;
    }
    if (addr == 0x7FFE) {
        const uint8_t banks4k = chrBanks ? uint8_t(chrBanks * 2) : 2;
        chrBank0 = uint8_t(data % (banks4k ? banks4k : 1));
        mappedAddr = 0;
        return true;
    }
    if (addr == 0x7FFF) {
        const uint8_t banks4k = chrBanks ? uint8_t(chrBanks * 2) : 2;
        chrBank1 = uint8_t(data % (banks4k ? banks4k : 1));
        mappedAddr = 0;
        return true;
    }

    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0x8000) {
        const uint8_t banks32k = prgBanks > 1 ? uint8_t(prgBanks / 2) : 1;
        prgBankSelect = banks32k ? uint8_t(data % banks32k) : 0;
        mappedAddr = 0;
        return true;
    }

    return false;
}

bool Mapper034::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
        } else if (addr <= 0x0FFF) {
            mappedAddr = uint32_t(chrBank0) * 0x1000u + uint32_t(addr & 0x0FFF);
        } else {
            mappedAddr = uint32_t(chrBank1) * 0x1000u + uint32_t(addr & 0x0FFF);
        }
        return true;
    }
    return false;
}

bool Mapper034::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
        } else if (addr <= 0x0FFF) {
            mappedAddr = uint32_t(chrBank0) * 0x1000u + uint32_t(addr & 0x0FFF);
        } else {
            mappedAddr = uint32_t(chrBank1) * 0x1000u + uint32_t(addr & 0x0FFF);
        }
        return chrBanks == 0;
    }
    return false;
}

void Mapper034::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgBankSelect);
    write_u8(out, chrBank0);
    write_u8(out, chrBank1);
}

bool Mapper034::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, prgBankSelect)) return false;
    if (p < end) {
        if (!read_u8(p, end, chrBank0)) return false;
        if (!read_u8(p, end, chrBank1)) return false;
    } else {
        chrBank0 = 0;
        chrBank1 = 1;
    }
    return true;
}
