#include "Mapper016.h"

#include <algorithm>
#include <cstring>

Mapper016::Mapper016(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper016::reset() {
    prgBank = 0;
    std::memset(chrBank, 0, sizeof(chrBank));
    mirrorMode = 0xFF;

    irqLatch = 0;
    irqCounter = 0;
    irqEnabled = false;
    irqActive = false;
}

bool Mapper016::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // $6000-$7FFF: treat as PRG-RAM (many dumps rely on it even if original board had EEPROM)
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    // $8000-$BFFF: 16 KiB switchable
    if (addr >= 0x8000 && addr <= 0xBFFF) {
        const uint32_t bank16 = uint32_t(prgBank % std::max<uint8_t>(1, prgBanks));
        mappedAddr = bank16 * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }

    // $C000-$FFFF: fixed to last
    if (addr >= 0xC000) {
        const uint32_t last16 = uint32_t(std::max<uint8_t>(1, prgBanks) - 1);
        mappedAddr = last16 * 0x4000u + uint32_t(addr & 0x3FFF);
        return true;
    }

    return false;
}

bool Mapper016::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;

    // $6000-$7FFF: PRG-RAM write
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    // Bandai regs exist either in $6000-$7FFF or $8000-$FFFF depending on submapper.
    // To be robust (and since many iNES headers omit submapper), accept both.
    const bool inRegRange = (addr >= 0x6000 && addr <= 0x7FFF) || (addr >= 0x8000);
    if (!inRegRange) return false;

    const uint16_t lo = addr & 0x000F;
    // CHR banks 0..7
    if (lo <= 0x0007) {
        chrBank[lo & 0x7] = data;
        return true;
    }
    // PRG bank
    if (lo == 0x0008) {
        prgBank = data & 0x0F;
        return true;
    }
    // Mirroring
    if (lo == 0x0009) {
        switch (data & 0x03) {
            case 0: mirrorMode = 1; break; // vertical
            case 1: mirrorMode = 0; break; // horizontal
            case 2: mirrorMode = 3; break; // 1-screen, low
            case 3: mirrorMode = 4; break; // 1-screen, high
        }
        return true;
    }
    // IRQ control (acknowledge + enable/disable). On LZ93D50, this also loads latch->counter.
    if (lo == 0x000A) {
        irqActive = false;
        irqEnabled = (data & 0x01) != 0;
        // Copy latch to counter (safe for both chip families; FCG games disable before changing)
        irqCounter = irqLatch;
        // If enabled while counter is 0, IRQ immediately
        if (irqEnabled && irqCounter == 0) {
            irqActive = true;
        }
        return true;
    }
    // IRQ latch low/high (little endian)
    if (lo == 0x000B) {
        irqLatch = uint16_t((irqLatch & 0xFF00) | uint16_t(data));
        return true;
    }
    if (lo == 0x000C) {
        irqLatch = uint16_t((uint16_t(data) << 8) | (irqLatch & 0x00FF));
        return true;
    }

    // $D is EEPROM control on some submappers; ignore.
    return true;
}

bool Mapper016::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        const uint16_t chrCount = (chrBanks == 0) ? 8u : uint16_t(chrBanks) * 8u;
        const uint8_t bank1k = uint8_t((addr >> 10) & 0x07);
        const uint16_t sel = uint16_t(chrBank[bank1k]) % std::max<uint16_t>(1, chrCount);
        mappedAddr = uint32_t(sel) * 0x0400u + uint32_t(addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper016::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            // CHR-RAM
            const uint8_t bank1k = uint8_t((addr >> 10) & 0x07);
            mappedAddr = uint32_t(chrBank[bank1k]) * 0x0400u + uint32_t(addr & 0x03FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper016::cpuClock() {
    if (!irqEnabled) return;

    if (irqCounter == 0) {
        // Holding at 0 triggers IRQ
        irqActive = true;
        return;
    }

    irqCounter--;
    if (irqCounter == 0) {
        irqActive = true;
    }
}
