#include "Mapper033.h"

Mapper033::Mapper033(uint8_t prgBanks16k, uint8_t chrBanks8k)
    : Mapper(prgBanks16k, chrBanks8k) {
    prgBanks8k = uint32_t(prgBanks16k) * 2u;
}

void Mapper033::reset() {
    prgReg0 = 0;
    prgReg1 = 1;
    chr2k0 = 0;
    chr2k1 = 1;
    chr1k[0] = 0;
    chr1k[1] = 1;
    chr1k[2] = 2;
    chr1k[3] = 3;
    mirrorMode = 0xFF;
}

bool Mapper033::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        const uint32_t bank = (addr & 0xE000) >> 13; // 0..3 in 8 KiB steps
        const uint32_t offset = addr & 0x1FFF;
        const uint32_t lastBank = (prgBanks8k == 0) ? 0 : (prgBanks8k - 1);
        const uint32_t secondLast = (prgBanks8k < 2) ? 0 : (prgBanks8k - 2);

        uint32_t sel = 0;
        switch (bank) {
            case 0: sel = prgReg0; break;
            case 1: sel = prgReg1; break;
            case 2: sel = secondLast; break;
            case 3: sel = lastBank; break;
            default: sel = 0; break;
        }
        if (prgBanks8k) sel %= prgBanks8k;
        mappedAddr = sel * 0x2000u + offset;
        return true;
    }
    return false;
}

bool Mapper033::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)mappedAddr;
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        const uint16_t r = addr & 0xA003;
        switch (r) {
            case 0x8000: {
                // .MPP PPPP: M=mirroring, P=PRG reg0
                const bool m = (data & 0x40) != 0;
                mirrorMode = m ? 0 : 1; // 0=H, 1=V
                prgReg0 = data & 0x3F;
            } break;
            case 0x8001:
                prgReg1 = data & 0x3F;
                break;
            case 0x8002:
                chr2k0 = uint16_t(data);
                break;
            case 0x8003:
                chr2k1 = uint16_t(data);
                break;
            case 0xA000:
                chr1k[0] = data;
                break;
            case 0xA001:
                chr1k[1] = data;
                break;
            case 0xA002:
                chr1k[2] = data;
                break;
            case 0xA003:
                chr1k[3] = data;
                break;
            default:
                break;
        }
        return true;
    }
    return false;
}

bool Mapper033::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
            return true;
        }

        if (addr < 0x0800) {
            mappedAddr = uint32_t(chr2k0) * 0x0800u + (addr & 0x07FF);
            return true;
        }
        if (addr < 0x1000) {
            mappedAddr = uint32_t(chr2k1) * 0x0800u + (addr & 0x07FF);
            return true;
        }
        const uint16_t sub = (addr - 0x1000) >> 10; // 0..3
        mappedAddr = uint32_t(chr1k[sub]) * 0x0400u + (addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper033::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
