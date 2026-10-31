#include "Mapper011.h"

// -----------------------------------------------------------------------------
// Mapper011 (Color Dreams)
// -----------------------------------------------------------------------------
void Mapper011::reset() {
    prgBankSelect = 0;
    chrBankSelect = 0;
}

bool Mapper011::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        // 32KB PRG banking
        if (prgBanks <= 1) {
            mappedAddr = uint32_t(addr & 0x3FFF);
            return true;
        }
        uint8_t banks32k = (prgBanks > 1) ? (prgBanks / 2) : 0;
        if (banks32k == 0) {
            mappedAddr = addr & 0x7FFF;
            return true;
        }
        uint8_t sel = prgBankSelect % banks32k;
        mappedAddr = (uint32_t(sel) * 0x8000) + (addr & 0x7FFF);
        return true;
    }
    return false;
}

bool Mapper011::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        // 7  bit  0
        // ---- ----
        // CCCC LLPP
        //       ||
        //       ++- Select 32 KB PRG ROM bank
        // CCCC------ Select 8 KB CHR ROM bank
        prgBankSelect = data & 0x03;
        chrBankSelect = (data >> 4) & 0x0F;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper011::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // 8KB CHR banking
        uint16_t banks8k = chrBanks ? chrBanks : 1;
        uint8_t sel = uint8_t(chrBankSelect % banks8k);
        mappedAddr = (uint32_t(sel) * 0x2000) + addr;
        return true;
    }
    return false;
}

bool Mapper011::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        // If CHR RAM, allow writes
        if (chrBanks == 0) {
            mappedAddr = addr & 0x1FFF;
            return true;
        }
        return false;
    }
    return false;
}
