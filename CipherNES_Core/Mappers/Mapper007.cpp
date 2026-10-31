#include "Mapper007.h"

#include <algorithm>

Mapper007::Mapper007(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper007::reset() {
    prgBank32k = 0;
    mirrorMode = 0xFF; // let header mirroring apply until first write
}

bool Mapper007::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        // 32KB banking ($8000-$FFFF)
        if (prgBanks <= 1) {
            mappedAddr = uint32_t(addr & 0x3FFF);
            return true;
        }
        const uint32_t banks32k = std::max<uint32_t>(1, uint32_t(prgBanks) / 2u);
        const uint32_t bank = uint32_t(prgBank32k) % banks32k;
        mappedAddr = bank * 0x8000u + uint32_t(addr & 0x7FFF);
        return true;
    }
    return false;
}

bool Mapper007::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        // Bits 0-2: 32KB PRG bank select
        prgBank32k = data & 0x07;
        // Bit 4: one-screen mirroring select (0=NT0, 1=NT1)
        mirrorMode = (data & 0x10) ? 4 : 3;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper007::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // AxROM boards typically use CHR-RAM (chrBanks==0) or fixed CHR.
        mappedAddr = addr;
        return true;
    }
    return false;
}

bool Mapper007::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
            return true;
        }
        return false;
    }
    return false;
}
