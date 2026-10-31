// Created by Jacob Hodgkins
// Action 53 multicart mapper (iNES mapper 28)

#include "Mapper028.h"
#include "../state_io.h"

void Mapper028::reset() {
    // Action 53 mapper state is unchanged by the console Reset signal.
    // The field initializers establish a valid power-up state with the last
    // 16 KiB bank visible at $C000-$FFFF.
}

uint16_t Mapper028::normalizePrgBank(int bank) const {
    const uint16_t count = prgBanks ? prgBanks : 1u;
    int normalized = bank % int(count);
    if (normalized < 0) normalized += count;
    return uint16_t(normalized);
}

void Mapper028::calculatePrgBanks(uint16_t& low, uint16_t& high) const {
    int prgLow = 0;
    int prgHigh = 0;
    const int out = int(outerBank) << 1;

    switch (mode & 0x3Cu) {
        case 0x00:
        case 0x04:
            prgLow = out;
            prgHigh = out | 1;
            break;
        case 0x10:
        case 0x14:
            prgLow = (out & ~2) | ((int(innerPrgBank) << 1) & 2);
            prgHigh = prgLow | 1;
            break;
        case 0x20:
        case 0x24:
            prgLow = (out & ~6) | ((int(innerPrgBank) << 1) & 6);
            prgHigh = prgLow | 1;
            break;
        case 0x30:
        case 0x34:
            prgLow = (out & ~14) | ((int(innerPrgBank) << 1) & 14);
            prgHigh = prgLow | 1;
            break;

        case 0x08:
            prgLow = out;
            prgHigh = out | (innerPrgBank & 1);
            break;
        case 0x18:
            prgLow = out;
            prgHigh = (out & ~2) | (innerPrgBank & 3);
            break;
        case 0x28:
            prgLow = out;
            prgHigh = (out & ~6) | (innerPrgBank & 7);
            break;
        case 0x38:
            prgLow = out;
            prgHigh = (out & ~14) | (innerPrgBank & 15);
            break;

        case 0x0C:
            prgLow = out | (innerPrgBank & 1);
            prgHigh = out | 1;
            break;
        case 0x1C:
            prgLow = (out & ~2) | (innerPrgBank & 3);
            prgHigh = out | 1;
            break;
        case 0x2C:
            prgLow = (out & ~6) | (innerPrgBank & 7);
            prgHigh = out | 1;
            break;
        case 0x3C:
            prgLow = (out & ~14) | (innerPrgBank & 15);
            prgHigh = out | 1;
            break;
    }

    low = normalizePrgBank(prgLow);
    high = normalizePrgBank(prgHigh);
}

bool Mapper028::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000) return false;

    uint16_t low = 0;
    uint16_t high = 0;
    calculatePrgBanks(low, high);
    const uint16_t bank = addr < 0xC000 ? low : high;
    mappedAddr = uint32_t(bank) * 0x4000u + uint32_t(addr & 0x3FFFu);
    return true;
}

void Mapper028::updateMirroringFromBankValue(uint8_t value) {
    if ((mode & 0x02u) == 0) {
        mode = uint8_t((mode & 0xFEu) | ((value >> 4) & 0x01u));
    }
}

bool Mapper028::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x5000 && addr <= 0x5FFF) {
        selectedRegister = data & 0x81u;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFFu);
        return true;
    }
    if (addr < 0x8000) return false;

    switch (selectedRegister) {
        case 0x00:
            chrBank = data & 0x03u;
            updateMirroringFromBankValue(data);
            break;
        case 0x01:
            innerPrgBank = data & 0x0Fu;
            updateMirroringFromBankValue(data);
            break;
        case 0x80:
            mode = data & 0x3Fu;
            break;
        case 0x81:
            outerBank = data & 0x3Fu;
            break;
        default:
            break;
    }

    mappedAddr = 0;
    return true;
}

bool Mapper028::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;

    const uint16_t count = chrBanks ? chrBanks : 4u;
    const uint16_t bank = uint16_t(chrBank) % count;
    mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr);
    return true;
}

bool Mapper028::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (!ppuMapRead(addr, mappedAddr)) return false;
    return chrBanks == 0;
}

uint8_t Mapper028::mirroringOverride() const {
    switch (mode & 0x03u) {
        case 0: return 3; // One-screen lower
        case 1: return 4; // One-screen upper
        case 2: return 1; // Vertical
        case 3: return 0; // Horizontal
    }
    return 0xFF;
}

void Mapper028::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, selectedRegister);
    write_u8(out, chrBank);
    write_u8(out, innerPrgBank);
    write_u8(out, mode);
    write_u8(out, outerBank);
}

bool Mapper028::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, selectedRegister) &&
           read_u8(p, end, chrBank) &&
           read_u8(p, end, innerPrgBank) &&
           read_u8(p, end, mode) &&
           read_u8(p, end, outerBank);
}
