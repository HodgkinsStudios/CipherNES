#include "Mapper066.h"

// -----------------------------------------------------------------------------
// Mapper066 (GxROM / GNROM)
// -----------------------------------------------------------------------------
void Mapper066::reset() {
    prgBankSelect = 0;
    chrBankSelect = 0;
}

bool Mapper066::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
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

bool Mapper066::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    if (addr >= 0x8000) {
        // 7  bit  0
        // ---- ----
        // xxPP xxCC
        //   ||   ||
        //   ||   ++- Select 8 KB CHR ROM bank
        //   ++------ Select 32 KB PRG ROM bank
        prgBankSelect = (data >> 4) & 0x03;
        chrBankSelect = data & 0x03;
        mappedAddr = 0;
        return true;
    }
    return false;
}

bool Mapper066::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // 8KB CHR banking
        uint16_t banks8k = chrBanks ? chrBanks : 1;
        uint8_t sel = uint8_t(chrBankSelect % banks8k);
        mappedAddr = (uint32_t(sel) * 0x2000) + addr;
        return true;
    }
    return false;
}

bool Mapper066::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            // Treat as fixed 8KB CHR RAM
            mappedAddr = addr & 0x1FFF;
            return true;
        }
        return false;
    }
    return false;
}
