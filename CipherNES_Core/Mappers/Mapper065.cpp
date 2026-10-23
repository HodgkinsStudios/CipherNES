#include "Mapper065.h"

#include <algorithm>

Mapper065::Mapper065(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

uint8_t Mapper065::prgBankCount8k() const {
    return uint8_t(prgBanks * 2);
}

uint16_t Mapper065::chrBankCount1k() const {
    if (chrBanks == 0) return 8;
    return uint16_t(chrBanks * 8);
}

void Mapper065::reset() {
    // Powerup state relied upon by some games
    prgReg0 = 0x00;
    prgReg1 = 0x01;
    prgSwap = false;

    for (auto &r : chrReg) r = 0;

    mirrorMode = 0xFF;

    irqEnabled = false;
    irqActive = false;
    irqReload = 0;
    irqCounter = 0;
}

bool Mapper065::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr < 0x8000) return false;

    const uint8_t prgCount = std::max<uint8_t>(1, prgBankCount8k());
    const uint8_t last = uint8_t(prgCount - 1);
    const uint8_t secondLast = uint8_t(prgCount >= 2 ? prgCount - 2 : 0);

    uint8_t bank = 0;
    if (addr >= 0x8000 && addr <= 0x9FFF) {
        bank = prgSwap ? secondLast : (prgReg0 % prgCount);
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        bank = prgReg1 % prgCount;
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        bank = prgSwap ? (prgReg0 % prgCount) : secondLast;
    } else { // $E000-$FFFF
        bank = last;
    }

    mappedAddr = uint32_t(bank) * 0x2000u + uint32_t(addr & 0x1FFF);
    return true;
}

bool Mapper065::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;

    if (addr == 0x8000) {
        prgReg0 = data;
        return true;
    }
    if (addr == 0xA000) {
        prgReg1 = data;
        return true;
    }

    if (addr >= 0xB000 && addr <= 0xB007) {
        chrReg[addr & 0x0007] = data;
        return true;
    }

    if (addr == 0x9000) {
        // PRG bank layout select: %0 => $8000 uses prgReg0, $C000 fixed to second-last
        //                         %1 => $C000 uses prgReg0, $8000 fixed to second-last
        prgSwap = (data & 0x80) != 0;
        return true;
    }

    if (addr == 0x9001) {
        // Mirroring: bits 6-7
        const uint8_t m = (data >> 6) & 0x03;
        // Cartridge::Mirror expects 0=H,1=V,2=4sc,3=1scA,4=1scB
        if (m == 0x00) mirrorMode = 1;        // vertical
        else if (m == 0x02) mirrorMode = 0;   // horizontal
        else mirrorMode = 3;                  // one-screen A
        return true;
    }

    if (addr == 0x9003) {
        // IRQ enable in bit 7. Any write acknowledges pending IRQ.
        irqActive = false;
        irqEnabled = (data & 0x80) != 0;
        return true;
    }

    if (addr == 0x9004) {
        // Reload counter from reload value; also acknowledges pending IRQ.
        irqActive = false;
        irqCounter = irqReload;
        return true;
    }

    if (addr == 0x9005) {
        // High 8 bits of reload
        irqReload = uint16_t((irqReload & 0x00FF) | (uint16_t(data) << 8));
        return true;
    }

    if (addr == 0x9006) {
        // Low 8 bits of reload
        irqReload = uint16_t((irqReload & 0xFF00) | uint16_t(data));
        return true;
    }

    // Unused/unknown registers are treated as handled.
    if (addr >= 0x8000) {
        return true;
    }

    return false;
}

bool Mapper065::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());
        const uint8_t bank = uint8_t((addr >> 10) & 0x07);
        const uint16_t b = uint16_t(chrReg[bank]) % chrCount;
        mappedAddr = uint32_t(b) * 0x0400u + uint32_t(addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper065::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            // CHR RAM
            const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());
            const uint8_t bank = uint8_t((addr >> 10) & 0x07);
            const uint16_t b = uint16_t(chrReg[bank]) % chrCount;
            mappedAddr = uint32_t(b) * 0x0400u + uint32_t(addr & 0x03FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper065::cpuClock() {
    if (!irqEnabled) return;
    if (irqCounter == 0) return; // stops at 0

    irqCounter--;
    if (irqCounter == 0) {
        irqActive = true;
    }
}
