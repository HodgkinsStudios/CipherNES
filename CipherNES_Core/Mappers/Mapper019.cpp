#include "Mapper019.h"

#include <algorithm>
#include <cstring>

Mapper019::Mapper019(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) {
    reset();
}

void Mapper019::reset() {
    std::memset(prgBank8k, 0, sizeof(prgBank8k));
    std::memset(chrBank1k, 0, sizeof(chrBank1k));
    wramProtect = 0x00;
    irqCounter = 0;
    irqEnabled = false;
    irqActive = false;
}

bool Mapper019::wramWriteEnabled(uint16_t addr) const {
    // Per nesdev: values outside $40-$4E -> all read-only
    if (wramProtect < 0x40 || wramProtect > 0x4E) return false;

    // 2KB windows: $6000-$67FF (A), $6800-$6FFF (B), $7000-$77FF (C), $7800-$7FFF (D)
    const uint16_t off = addr - 0x6000;
    const uint8_t win = uint8_t(off / 0x0800);
    const uint8_t bit = uint8_t(1u << win); // A=bit0

    // If window bit is 1 => write-protect. 0 => write enabled.
    return (wramProtect & bit) == 0;
}

bool Mapper019::cpuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    // WRAM
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    const uint8_t prgCount = std::max<uint8_t>(1, prgBankCount8k());
    const uint8_t last = uint8_t(prgCount - 1);

    if (addr >= 0x8000 && addr <= 0x9FFF) {
        const uint8_t b = uint8_t(prgBank8k[0] % prgCount);
        mappedAddr = uint32_t(b) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xA000 && addr <= 0xBFFF) {
        const uint8_t b = uint8_t(prgBank8k[1] % prgCount);
        mappedAddr = uint32_t(b) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xC000 && addr <= 0xDFFF) {
        const uint8_t b = uint8_t(prgBank8k[2] % prgCount);
        mappedAddr = uint32_t(b) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }
    if (addr >= 0xE000) {
        mappedAddr = uint32_t(last) * 0x2000u + uint32_t(addr & 0x1FFF);
        return true;
    }

    // Reads from $5000/$5800 return counter values (open-bus beyond that is ignored here)
    if ((addr & 0xF800) == 0x5000) {
        mappedAddr = 0;
        return false;
    }
    if ((addr & 0xF800) == 0x5800) {
        mappedAddr = 0;
        return false;
    }

    return false;
}

bool Mapper019::cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) {
    mappedAddr = 0;

    // WRAM writes (honor protect)
    if (addr >= 0x6000 && addr <= 0x7FFF) {
        if (!wramWriteEnabled(addr)) return true; // handled but ignored
        mappedAddr = PRG_RAM_FLAG | uint32_t(addr & 0x1FFF);
        return true;
    }

    const uint16_t reg = addr & 0xF800;

    // IRQ counter low ($5000)
    if (reg == 0x5000) {
        irqCounter = uint16_t((irqCounter & 0x7F00) | uint16_t(data));
        irqActive = false;
        return true;
    }
    // IRQ counter high / enable ($5800)
    if (reg == 0x5800) {
        irqEnabled = (data & 0x80) != 0;
        irqCounter = uint16_t(((uint16_t(data & 0x7F) << 8) | (irqCounter & 0x00FF)) & 0x7FFF);
        irqActive = false;
        return true;
    }

    // CHR & NT select ($8000-$DFFF), 12 banks of 1KB. We only use first 8 (pattern tables).
    if (addr >= 0x8000 && addr <= 0xDFFF) {
        const uint8_t bank = uint8_t((addr - 0x8000) / 0x0800);
        if (bank < 8) {
            chrBank1k[bank] = data;
        }
        return true;
    }

    // PRG select 1 ($E000) => $8000
    if (reg == 0xE000) {
        prgBank8k[0] = data & 0x3F;
        return true;
    }
    // PRG select 2 ($E800) => $A000
    if (reg == 0xE800) {
        prgBank8k[1] = data & 0x3F;
        return true;
    }
    // PRG select 3 ($F000) => $C000
    if (reg == 0xF000) {
        prgBank8k[2] = data & 0x3F;
        return true;
    }
    // Write protect ($F800)
    if (reg == 0xF800) {
        wramProtect = data;
        return true;
    }

    // Other registers (sound, chip RAM port) ignored.
    return false;
}

bool Mapper019::ppuMapRead(uint16_t addr, uint32_t& mappedAddr) {
    if (addr <= 0x1FFF) {
        const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());
        const uint8_t bank = uint8_t((addr >> 10) & 0x07);
        const uint16_t sel = uint16_t(chrBank1k[bank]) % chrCount;
        mappedAddr = uint32_t(sel) * 0x0400u + uint32_t(addr & 0x03FF);
        return true;
    }
    return false;
}

bool Mapper019::ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t /*data*/) {
    if (addr <= 0x1FFF) {
        if (chrBanks == 0) {
            const uint16_t chrCount = std::max<uint16_t>(1, chrBankCount1k());
            const uint8_t bank = uint8_t((addr >> 10) & 0x07);
            const uint16_t sel = uint16_t(chrBank1k[bank]) % chrCount;
            mappedAddr = uint32_t(sel) * 0x0400u + uint32_t(addr & 0x03FF);
            return true;
        }
        return false;
    }
    return false;
}

void Mapper019::cpuClock() {
    if (!irqEnabled) return;
    if (irqCounter >= 0x7FFF) {
        irqActive = true;
        irqEnabled = false;
        return;
    }
    irqCounter = uint16_t((irqCounter + 1) & 0x7FFF);
    if (irqCounter == 0x7FFF) {
        irqActive = true;
        irqEnabled = false;
    }
}
