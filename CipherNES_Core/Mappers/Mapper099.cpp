#include "Mapper099.h"

Mapper099::Mapper099(uint8_t prgBanks16k, uint8_t chrBanks8k)
    : Mapper(prgBanks16k, chrBanks8k) {
    prgBanks8k = uint32_t(prgBanks16k) * 2u;
}

void Mapper099::reset() {
    bankSel = 0;
}

bool Mapper099::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        const uint32_t offset = addr & 0x1FFF;
        const uint32_t bankRegion = (addr & 0xE000) >> 13; // 0..3

        const uint32_t lastBank = (prgBanks8k == 0) ? 0 : (prgBanks8k - 1);
        const uint32_t fixedBase = (prgBanks8k >= 3) ? (prgBanks8k - 3) : 0;

        uint32_t sel = 0;
        if (bankRegion == 0) {
            // $8000-$9FFF switchable bank; if there is an alternate bank (40 KiB variants), use bank #4.
            if (bankSel && prgBanks8k > 4) sel = 4;
            else sel = 0;
        } else {
            // Fixed last 24 KiB
            sel = fixedBase + (bankRegion - 1);
            if (sel > lastBank) sel = lastBank;
        }
        if (prgBanks8k) sel %= prgBanks8k;
        mappedAddr = sel * 0x2000u + offset;
        return true;
    }
    return false;
}

bool Mapper099::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)mappedAddr;
    if (addr == 0x4016) {
        bankSel = (data >> 2) & 0x01;
        return true;
    }
    return false;
}

bool Mapper099::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
            return true;
        }
        const uint32_t banks = uint32_t(chrBanks);
        const uint32_t bank = uint32_t(bankSel);
        if (bank >= banks) {
            // Open bus modeled as out-of-range read (Cartridge returns 0)
            mappedAddr = banks * 0x2000u;
            return true;
        }
        mappedAddr = bank * 0x2000u + addr;
        return true;
    }
    return false;
}

bool Mapper099::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
