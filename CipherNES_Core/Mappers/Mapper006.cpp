#include "Mapper006.h"

#include "../state_io.h"

Mapper006::Mapper006(uint16_t prgBanks,
                     uint16_t chrBanks,
                     uint16_t mapperNumber)
    : Mapper(prgBanks, chrBanks), mapperNumber(mapperNumber) {
    reset();
}

void Mapper006::reset() {
    prg16[0] = 0;
    if (mapperNumber == 8) {
        // F3xxx powers up with the first 32 KiB visible. The upper half is not
        // affected by its normal bank register.
        prg16[1] = prgBanks > 1 ? 1u : 0u;
    } else {
        prg16[1] = prgBanks ? uint16_t(prgBanks - 1u) : 0u;
    }
    chr8 = 0;
    irqCounter = 0;
    mirrorMode = 0xFF;
    irqEnabled = false;
    irqActive = false;
    ffeAltMode = true;
}

bool Mapper006::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;
    const uint16_t count = prgBanks ? prgBanks : 1u;
    const uint8_t slot = uint8_t((addr - 0x8000) >> 14);
    mappedAddr = uint32_t(prg16[slot] % count) * 0x4000u
               + uint32_t(addr & 0x3FFFu);
    return true;
}

bool Mapper006::cpuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t data) {
    switch (addr) {
        case 0x42FE:
            ffeAltMode = (data & 0x80u) == 0;
            mirrorMode = (data & 0x10u) ? 4u : 3u;
            mappedAddr = 0;
            return true;
        case 0x42FF:
            mirrorMode = (data & 0x10u) ? 0u : 1u;
            mappedAddr = 0;
            return true;
        case 0x4501:
            irqEnabled = false;
            irqActive = false;
            mappedAddr = 0;
            return true;
        case 0x4502:
            irqCounter = uint16_t((irqCounter & 0xFF00u) | data);
            irqActive = false;
            mappedAddr = 0;
            return true;
        case 0x4503:
            irqCounter = uint16_t((irqCounter & 0x00FFu)
                                | (uint16_t(data) << 8));
            irqEnabled = true;
            irqActive = false;
            mappedAddr = 0;
            return true;
        default:
            break;
    }

    if (addr < 0x8000) return false;

    if (mapperNumber == 8) {
        prg16[0] = uint16_t((data & 0xF8u) >> 3);
        chr8 = uint16_t(data & 0x07u);
    } else {
        uint8_t chrValue = data;
        if (chrBanks == 0 || ffeAltMode) {
            prg16[0] = uint16_t((data & 0xFCu) >> 2);
            chrValue = uint8_t(data & 0x03u);
        }
        chr8 = chrValue;
    }

    mappedAddr = 0;
    return true;
}

bool Mapper006::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    const uint16_t count = chrBanks ? chrBanks : 4u; // FFE boards expose 32 KiB CHR RAM.
    mappedAddr = uint32_t(chr8 % count) * 0x2000u + addr;
    return true;
}

bool Mapper006::ppuMapWrite(uint16_t addr,
                            uint32_t& mappedAddr,
                            uint8_t /*data*/) {
    return chrBanks == 0 && ppuMapRead(addr, mappedAddr);
}

void Mapper006::cpuClock() {
    if (!irqEnabled) return;
    ++irqCounter;
    if (irqCounter == 0) {
        irqActive = true;
        irqEnabled = false;
    }
}

void Mapper006::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u16(out, prg16[0]);
    write_u16(out, prg16[1]);
    write_u16(out, chr8);
    write_u16(out, irqCounter);
    write_u8(out, mirrorMode);
    write_bool(out, irqEnabled);
    write_bool(out, irqActive);
    write_bool(out, ffeAltMode);
}

bool Mapper006::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u16(p, end, prg16[0])
        && read_u16(p, end, prg16[1])
        && read_u16(p, end, chr8)
        && read_u16(p, end, irqCounter)
        && read_u8(p, end, mirrorMode)
        && read_bool(p, end, irqEnabled)
        && read_bool(p, end, irqActive)
        && read_bool(p, end, ffeAltMode);
}
