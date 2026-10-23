#include "Mapper009.h"

// MMC2 / Mapper 9 reference:
// https://www.nesdev.org/wiki/MMC2

Mapper009::Mapper009(uint8_t prgBanks, uint8_t chrBanks)
    : Mapper(prgBanks, chrBanks) {}

void Mapper009::reset() {
    prgBank8k = 0;

    chrFD_0000 = 0;
    chrFE_0000 = 0;
    chrFD_1000 = 0;
    chrFE_1000 = 0;

    // Power-on state is not well-defined electrically; in practice games
    // will hit the trigger addresses very early. Default both to $FE.
    latch0 = 0xFE;
    latch1 = 0xFE;

    mirrorMode = 1; // vertical
}

bool Mapper009::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // PRG-RAM $6000-$7FFF (PlayChoice variants). We always expose it.
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    const uint8_t banks8k = prg8kBankCount();
    if (banks8k == 0) return false;

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        // Switchable 8 KB at $8000-$9FFF
        uint8_t bank = prgBank8k % banks8k;
        mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xA000 && addr <= 0xBFFF) {
        // Fixed to last 24 KB: bank -3
        uint8_t bank = (banks8k >= 3) ? uint8_t(banks8k - 3) : 0;
        mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xC000 && addr <= 0xDFFF) {
        // Fixed: bank -2
        uint8_t bank = (banks8k >= 2) ? uint8_t(banks8k - 2) : 0;
        mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }

    if (addr >= 0xE000) {
        // Fixed: bank -1
        uint8_t bank = (banks8k >= 1) ? uint8_t(banks8k - 1) : 0;
        mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }

    return false;
}

bool Mapper009::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    // PRG-RAM write
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = Mapper::PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        (void)data;
        return true;
    }

    // Registers ($A000-$FFFF)
    if (addr >= 0xA000 && addr <= 0xAFFF) {
        // PRG bank select (8 KB at $8000)
        prgBank8k = data & 0x0F;
        if (prg8kBankCount()) prgBank8k %= prg8kBankCount();
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xB000 && addr <= 0xBFFF) {
        chrFD_0000 = data & 0x1F;
        if (chr4kBankCount()) chrFD_0000 %= chr4kBankCount();
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xCFFF) {
        chrFE_0000 = data & 0x1F;
        if (chr4kBankCount()) chrFE_0000 %= chr4kBankCount();
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xD000 && addr <= 0xDFFF) {
        chrFD_1000 = data & 0x1F;
        if (chr4kBankCount()) chrFD_1000 %= chr4kBankCount();
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xE000 && addr <= 0xEFFF) {
        chrFE_1000 = data & 0x1F;
        if (chr4kBankCount()) chrFE_1000 %= chr4kBankCount();
        mappedAddr = 0;
        return true;
    }
    if (addr >= 0xF000) {
        // Mirroring: 0 = vertical, 1 = horizontal
        const uint8_t m = data & 0x01;
        mirrorMode = (m == 0) ? 1 : 0;
        mappedAddr = 0;
        return true;
    }

    return false;
}

uint32_t Mapper009::mapChr4k(uint8_t bank4k, uint16_t addr) const {
    const uint32_t base = uint32_t(bank4k) * 0x1000u;
    return base + uint32_t(addr & 0x0FFF);
}

bool Mapper009::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr > 0x1FFF) return false;

    if (chrBanks == 0) {
        // CHR RAM
        mappedAddr = addr;
        return true;
    }

    if (addr <= 0x0FFF) {
        // $0000-$0FFF uses latch0
        const uint8_t bank = (latch0 == 0xFD) ? chrFD_0000 : chrFE_0000;
        mappedAddr = mapChr4k(bank, addr);
        return true;
    }

    // $1000-$1FFF uses latch1
    const uint8_t bank = (latch1 == 0xFD) ? chrFD_1000 : chrFE_1000;
    mappedAddr = mapChr4k(bank, addr);
    return true;
}

bool Mapper009::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr > 0x1FFF) return false;
    mappedAddr = addr;
    return chrBanks == 0; // only allow writes when CHR RAM
}

void Mapper009::ppuAddressLatch(uint16_t addr) {
    // Mask to pattern table space
    addr &= 0x1FFF;

    // Latch 0 triggers
    if (addr == 0x0FD8) {
        latch0 = 0xFD;
        return;
    }
    if (addr == 0x0FE8) {
        latch0 = 0xFE;
        return;
    }

    // Latch 1 triggers (ranges)
    if (addr >= 0x1FD8 && addr <= 0x1FDF) {
        latch1 = 0xFD;
        return;
    }
    if (addr >= 0x1FE8 && addr <= 0x1FEF) {
        latch1 = 0xFE;
        return;
    }
}
