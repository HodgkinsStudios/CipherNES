#include "Mapper079.h"

Mapper079::Mapper079(uint8_t prgBanks16k, uint8_t chrBanks8k)
    : Mapper(prgBanks16k, chrBanks8k) {
    prgBanks32k = uint32_t(prgBanks16k) / 2u;
    if (prgBanks32k == 0) prgBanks32k = 1;
}

void Mapper079::reset() {
    prg32kBank = 0;
    chr8kBank = 0;
}

bool Mapper079::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        if (prgBanks <= 1) {
            mappedAddr = uint32_t(addr & 0x3FFF);
            return true;
        }
        const uint32_t bank = uint32_t(prg32kBank) % prgBanks32k;
        mappedAddr = bank * 0x8000u + uint32_t(addr - 0x8000);
        return true;
    }
    return false;
}

bool Mapper079::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)mappedAddr;
    // Control register is at $4100-$5FFF with bit8 set: 010x xxx1 xxxx xxxx
    if (addr >= 0x4100 && addr <= 0x5FFF && (addr & 0x0100) == 0x0100) {
        prg32kBank = (data >> 4) & 0x01;
        chr8kBank = data & 0x07;
        return true;
    }
    return false;
}

bool Mapper079::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
            return true;
        }
        const uint32_t bank = uint32_t(chr8kBank) % uint32_t(chrBanks ? chrBanks : 1);
        mappedAddr = bank * 0x2000u + addr;
        return true;
    }
    return false;
}

bool Mapper079::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
