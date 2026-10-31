#include "Mapper017.h"

Mapper017::Mapper017(uint8_t prgBanks16k, uint8_t chrBanks8k)
    : Mapper(prgBanks16k, chrBanks8k) {
    prgBanks8k = uint32_t(prgBanks16k) * 2u;
}

void Mapper017::reset() {
    chr1kMode = true;
    irqSourcePA12 = false;

    bankingEnabled = true;
    use2m = false;

    const uint8_t total8 = uint8_t(prgBanks8k ? prgBanks8k : 1);
    prg4m[0] = (total8 >= 4) ? uint8_t(total8 - 4) : 0;
    prg4m[1] = (total8 >= 3) ? uint8_t(total8 - 3) : 0;
    prg4m[2] = (total8 >= 2) ? uint8_t(total8 - 2) : 0;
    prg4m[3] = (total8 >= 1) ? uint8_t(total8 - 1) : 0;
    for (int i = 0; i < 4; i++) prg2m[i] = prg4m[i];

    chr8kBank = 0;
    for (int i = 0; i < 8; i++) chr1k[i] = uint8_t(i);

    irqEnabled = false;
    irqCounter = 0;
    lastA12 = false;
    irqActive = false;
    mirrorMode = 0xFF;
}

void Mapper017::setMirroringFrom42Fx(uint16_t addr) {
    const uint8_t mode = uint8_t(addr & 0x0003);
    switch (mode) {
        case 0: mirrorMode = 3; break; // 1SC LO
        case 1: mirrorMode = 4; break; // 1SC HI
        case 2: mirrorMode = 1; break; // V
        case 3: mirrorMode = 0; break; // H
    }
}

void Mapper017::clockIrq() {
    if (!irqEnabled) return;
    const uint16_t prev = irqCounter;
    irqCounter = uint16_t(irqCounter + 1);
    if (prev == 0xFFFF && irqCounter == 0x0000) {
        irqActive = true;
    }
}

void Mapper017::cpuClock() {
    if (!irqSourcePA12) clockIrq();
}

void Mapper017::ppuAddressLatch(uint16_t addr) {
    const bool a12 = (addr & 0x1000) != 0;
    if (irqSourcePA12 && a12 && !lastA12) {
        clockIrq();
    }
    lastA12 = a12;
}

bool Mapper017::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000) {
        const uint32_t slot = (uint32_t(addr - 0x8000) >> 13) & 0x03u;
        const uint32_t offset = addr & 0x1FFF;

        uint8_t bank = 0;
        if (bankingEnabled) {
            bank = use2m ? prg2m[slot] : prg4m[slot];
        } else {
            const uint32_t total8 = prgBanks8k ? prgBanks8k : 1;
            bank = uint8_t(slot + (total8 >= 4 ? (total8 - 4) : 0));
        }

        if (prgBanks8k) bank = uint8_t(uint32_t(bank) % prgBanks8k);
        mappedAddr = uint32_t(bank) * 0x2000u + offset;
        return true;
    }
    return false;
}

bool Mapper017::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    (void)mappedAddr;

    if (addr >= 0x42FC && addr <= 0x42FF) {
        setMirroringFrom42Fx(addr);
        return true;
    }

    if (addr >= 0x43FC && addr <= 0x43FF) {
        bankingEnabled = (addr & 0x0001) == 0;
        if (bankingEnabled) use2m = (addr & 0x0002) != 0;
        chr8kBank = data & 0x03;
        return true;
    }

    if (addr == 0x4500) {
        // PMWW ImNC: C=bit0, I=bit3
        chr1kMode = (data & 0x01) != 0;
        irqSourcePA12 = (data & 0x08) != 0;
        return true;
    }

    if (addr == 0x4501) {
        irqActive = false;
        irqEnabled = false;
        return true;
    }

    if (addr == 0x4502) {
        irqCounter = (irqCounter & 0xFF00u) | uint16_t(data);
        irqActive = false;
        return true;
    }

    if (addr == 0x4503) {
        irqCounter = (irqCounter & 0x00FFu) | (uint16_t(data) << 8);
        irqActive = false;
        irqEnabled = true;
        return true;
    }

    if (addr >= 0x4504 && addr <= 0x4507) {
        prg4m[int(addr - 0x4504)] = data & 0x3F;
        return true;
    }

    if (addr >= 0x4510 && addr <= 0x4517) {
        chr1k[int(addr - 0x4510)] = data;
        return true;
    }

    if (addr >= 0x8000) {
        const int slot = int((uint16_t(addr - 0x8000) >> 13) & 0x03);
        prg2m[slot] = (data >> 2) & 0x3F; // PPPPPPCC
        chr8kBank = data & 0x03;
        return true;
    }

    return false;
}

bool Mapper017::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            mappedAddr = addr;
            return true;
        }

        if (chr1kMode) {
            const uint8_t bank = chr1k[(addr >> 10) & 0x07];
            mappedAddr = uint32_t(bank) * 0x0400u + (addr & 0x03FF);
            return true;
        }

        const uint32_t banks = uint32_t(chrBanks);
        const uint32_t bank = banks ? (uint32_t(chr8kBank) % banks) : 0;
        mappedAddr = bank * 0x2000u + addr;
        return true;
    }
    return false;
}

bool Mapper017::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF && chrBanks == 0) {
        mappedAddr = addr;
        return true;
    }
    return false;
}
