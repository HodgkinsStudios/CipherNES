#include "Mapper067.h"

#include <algorithm>
#include <cstring>

Mapper067::Mapper067(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper067::reset() {
    prgBank = 0;
    std::memset(chrBank2k, 0, sizeof(chrBank2k));
    mirrorMode = 0xFF;

    irqCounter = 0;
    irqEnabled = false;
    irqActive = false;
    irqWriteHi = true;
}

bool Mapper067::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        const uint32_t bank16 = uint32_t(prgBank % std::max<uint8_t>(1, prgBanks));
        mappedAddr = bank16 * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    if (addr >= 0xC000) {
        const uint32_t last16 = uint32_t(std::max<uint8_t>(1, prgBanks) - 1);
        mappedAddr = last16 * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }
    return false;
}

bool Mapper067::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;

    // Registers are mirrored; use mask $F800 (see nesdev wiki)
    const uint16_t reg = addr & 0xF800;

    // IRQ acknowledge ($8000)
    if (reg == 0x8000) {
        irqActive = false;
        return true;
    }

    // CHR banks 0..3 ($8800-$B800)
    if (reg == 0x8800) { chrBank2k[0] = data & 0x3F; return true; }
    if (reg == 0x9800) { chrBank2k[1] = data & 0x3F; return true; }
    if (reg == 0xA800) { chrBank2k[2] = data & 0x3F; return true; }
    if (reg == 0xB800) { chrBank2k[3] = data & 0x3F; return true; }

    // IRQ load ($C800), write twice: high then low
    if (reg == 0xC800) {
        if (irqWriteHi) {
            irqCounter = uint16_t((uint16_t(data) << 8) | (irqCounter & 0x00FF));
            irqWriteHi = false;
        } else {
            irqCounter = uint16_t((irqCounter & 0xFF00) | uint16_t(data));
            irqWriteHi = true;
        }
        return true;
    }

    // IRQ enable ($D800) - bit4 enables counting; any write resets the $C800 hi/lo toggle
    if (reg == 0xD800) {
        irqEnabled = (data & 0x10) != 0;
        irqWriteHi = true;
        return true;
    }

    // Mirroring ($E800)
    if (reg == 0xE800) {
        switch (data & 0x03) {
            case 0: mirrorMode = 1; break; // vertical
            case 1: mirrorMode = 0; break; // horizontal
            case 2: mirrorMode = 3; break; // 1-screen low
            case 3: mirrorMode = 4; break; // 1-screen high
        }
        return true;
    }

    // PRG bank ($F800)
    if (reg == 0xF800) {
        prgBank = data & 0x0F;
        return true;
    }

    return false;
}

bool Mapper067::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        // 2KB banks
        const uint16_t chrCount2k = (chrBanks == 0) ? 4u : uint16_t(chrBanks) * 4u;
        const uint8_t bank2k = uint8_t((addr >> 11) & 0x03);
        const uint16_t sel = uint16_t(chrBank2k[bank2k]) % std::max<uint16_t>(1, chrCount2k);
        mappedAddr = uint32_t(sel) * 0x0800u + uint32_t(addr & 0x07FF);
        return true;
    }
    return false;
}

bool Mapper067::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            const uint8_t bank2k = uint8_t((addr >> 11) & 0x03);
            mappedAddr = uint32_t(chrBank2k[bank2k]) * 0x0800u + uint32_t(addr & 0x07FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper067::cpuClock() {
    if (!irqEnabled) return;

    // Decrement each CPU cycle. When it wraps $0000 -> $FFFF, fire and pause.
    if (irqCounter == 0x0000) {
        irqCounter = 0xFFFF;
        irqActive = true;
        irqEnabled = false;
        return;
    }
    irqCounter--;
}
