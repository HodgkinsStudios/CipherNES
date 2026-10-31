// Created by Jacob Hodgkins
// Konami VRC2a (iNES mapper 22)

#include "Mapper022.h"
#include "../state_io.h"

uint16_t Mapper022::prgBankCount8K() const {
    return prgBanks ? uint16_t(prgBanks * 2u) : 1u;
}

uint16_t Mapper022::chrBankCount1K() const {
    return chrBanks ? uint16_t(chrBanks * 8u) : 8u;
}

void Mapper022::reset() {
    prgBank0 = 0;
    prgBank1 = 1;
    for (uint8_t i = 0; i < chrRegisters.size(); ++i) {
        // Mapper 22 shifts the raw VRC2 CHR register right by one.
        chrRegisters[i] = uint8_t(i << 1);
    }
    mirrorMode = 0xFF;
}

bool Mapper022::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;

    const uint16_t bankCount = prgBankCount8K();
    uint16_t bank = 0;
    if (addr < 0xA000) {
        bank = uint16_t(prgBank0) % bankCount;
    } else if (addr < 0xC000) {
        bank = uint16_t(prgBank1) % bankCount;
    } else if (addr < 0xE000) {
        bank = bankCount >= 2 ? uint16_t(bankCount - 2) : 0;
    } else {
        bank = uint16_t(bankCount - 1);
    }

    mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFFu);
    return true;
}

void Mapper022::writeChrNibble(uint8_t logicalRegister, uint8_t data) {
    const uint8_t bank = uint8_t(logicalRegister >> 1);
    if (bank >= chrRegisters.size()) return;

    if ((logicalRegister & 1u) == 0) {
        chrRegisters[bank] = uint8_t((chrRegisters[bank] & 0xF0u) | (data & 0x0Fu));
    } else {
        chrRegisters[bank] = uint8_t((chrRegisters[bank] & 0x0Fu) | ((data & 0x0Fu) << 4));
    }
}

bool Mapper022::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr < 0x8000) return false;

    const uint16_t region = addr & 0xF000u;
    if (region == 0x8000u) {
        prgBank0 = data;
    } else if (region == 0x9000u) {
        // VRC2 supports vertical/horizontal mirroring only.
        mirrorMode = (data & 0x01u) ? 0u : 1u;
    } else if (region == 0xA000u) {
        prgBank1 = data;
    } else if (region >= 0xB000u && region <= 0xE000u) {
        // VRC2a uses A0/A1 in the order x000, x002, x001, x003.
        const uint8_t addressBits = uint8_t(addr & 0x0003u);
        const uint8_t logicalLow = uint8_t(((addressBits & 0x01u) << 1) |
                                           ((addressBits & 0x02u) >> 1));
        const uint8_t regionBase = uint8_t((region - 0xB000u) >> 10); // 0,4,8,12
        writeChrNibble(uint8_t(regionBase + logicalLow), data);
    }

    mappedAddr = 0;
    return true;
}

bool Mapper022::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;

    const uint8_t slot = uint8_t(addr >> 10);
    const uint16_t bankCount = chrBankCount1K();
    const uint16_t bank = uint16_t(chrRegisters[slot] >> 1) % bankCount;
    mappedAddr = uint32_t(bank) * 0x0400u + uint32_t(addr & 0x03FFu);
    return true;
}

bool Mapper022::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (!ppuMapRead(addr, mappedAddr)) return false;
    return chrBanks == 0;
}

void Mapper022::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgBank0);
    write_u8(out, prgBank1);
    for (uint8_t value : chrRegisters) write_u8(out, value);
    write_u8(out, mirrorMode);
}

bool Mapper022::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    if (!read_u8(p, end, prgBank0) || !read_u8(p, end, prgBank1)) return false;
    for (uint8_t& value : chrRegisters) {
        if (!read_u8(p, end, value)) return false;
    }
    return read_u8(p, end, mirrorMode);
}
