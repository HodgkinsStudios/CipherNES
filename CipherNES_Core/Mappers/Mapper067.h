#pragma once

#include "Mapper.h"

// iNES Mapper 067 - Sunsoft-3
// Banking:
//  - $8000-$BFFF: 16 KiB switchable PRG
//  - $C000-$FFFF: 16 KiB fixed to last
//  - $0000-$1FFF: 4 x 2 KiB CHR banks
// IRQ: 16-bit CPU-cycle down counter (write-twice load)

class Mapper067 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ; }
    Mapper067(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    bool irqState() const override { return irqActive; }
    void irqClear() override { irqActive = false; }

    bool usesCpuClock() const override { return true; }
    void cpuClock() override;

private:
    uint8_t prgBank = 0;
    uint8_t chrBank2k[4]{};
    uint8_t mirrorMode = 0xFF;

    // IRQ
    uint16_t irqCounter = 0;
    bool irqEnabled = false;
    bool irqActive = false;
    bool irqWriteHi = true;
};
