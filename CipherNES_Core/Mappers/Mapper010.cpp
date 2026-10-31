#include "Mapper010.h"
#include "../state_io.h"

void Mapper010::reset() {
    prgBank16k = 0;
    chrFD_0000 = chrFE_0000 = chrFD_1000 = chrFE_1000 = 0;
    latch0 = latch1 = 0xFE;
    mirrorMode = 1;
}

bool Mapper010::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        mappedAddr = uint32_t(prgBank16k % (prgBanks ? prgBanks : 1)) * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        const uint8_t bank = prgBanks ? uint8_t(prgBanks - 1) : 0;
        mappedAddr = uint32_t(bank) * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper010::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xA000 && addr <= 0xAFFF) {
        prgBank16k = data & 0x0F;
        if (prgBanks) prgBank16k %= prgBanks;
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xB000 && addr <= 0xBFFF) { chrFD_0000 = data & 0x1F; if (chr4kBankCount()) chrFD_0000 %= chr4kBankCount(); mappedAddr = 0; return true; }
    if (addr >= 0xC000 && addr <= 0xCFFF) { chrFE_0000 = data & 0x1F; if (chr4kBankCount()) chrFE_0000 %= chr4kBankCount(); mappedAddr = 0; return true; }
    if (addr >= 0xD000 && addr <= 0xDFFF) { chrFD_1000 = data & 0x1F; if (chr4kBankCount()) chrFD_1000 %= chr4kBankCount(); mappedAddr = 0; return true; }
    if (addr >= 0xE000 && addr <= 0xEFFF) { chrFE_1000 = data & 0x1F; if (chr4kBankCount()) chrFE_1000 %= chr4kBankCount(); mappedAddr = 0; return true; }
    if (addr >= 0xF000) { mirrorMode = (data & 0x01) ? 0 : 1; mappedAddr = 0; return true; }
    return false;
}

uint32_t Mapper010::mapChr4k(uint8_t bank4k, uint16_t addr) const {
    return uint32_t(bank4k) * 0x1000u + uint32_t(addr & 0x0FFF);
}

bool Mapper010::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;
    if (chrBanks == 0) { mappedAddr = addr; return true; }
    if (addr <= 0x0FFF) {
        mappedAddr = mapChr4k(latch0 == 0xFD ? chrFD_0000 : chrFE_0000, addr);
        return true;
    }
    mappedAddr = mapChr4k(latch1 == 0xFD ? chrFD_1000 : chrFE_1000, addr);
    return true;
}

bool Mapper010::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF) return false;
    mappedAddr = addr;
    return chrBanks == 0;
}

void Mapper010::ppuAddressLatch(uint16_t addr) {
    addr &= 0x1FFF;
    if (addr == 0x0FD8) { latch0 = 0xFD; return; }
    if (addr == 0x0FE8) { latch0 = 0xFE; return; }
    if (addr >= 0x1FD8 && addr <= 0x1FDF) { latch1 = 0xFD; return; }
    if (addr >= 0x1FE8 && addr <= 0x1FEF) { latch1 = 0xFE; return; }
}

void Mapper010::saveState(std::vector<uint8_t>& out) const {
    using namespace ciphernes_state;
    write_u8(out, prgBank16k); write_u8(out, chrFD_0000); write_u8(out, chrFE_0000);
    write_u8(out, chrFD_1000); write_u8(out, chrFE_1000); write_u8(out, latch0);
    write_u8(out, latch1); write_u8(out, mirrorMode);
}

bool Mapper010::loadState(const uint8_t*& p, const uint8_t* end) {
    using namespace ciphernes_state;
    return read_u8(p, end, prgBank16k) && read_u8(p, end, chrFD_0000) &&
           read_u8(p, end, chrFE_0000) && read_u8(p, end, chrFD_1000) &&
           read_u8(p, end, chrFE_1000) && read_u8(p, end, latch0) &&
           read_u8(p, end, latch1) && read_u8(p, end, mirrorMode);
}
