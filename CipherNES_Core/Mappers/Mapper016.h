#pragma once

#include "Mapper.h"

// iNES Mapper 016 - Bandai FCG-1/2 and LZ93D50 family (basic banking + cycle IRQ)
// This implementation focuses on the common features used by commercial titles:
//  - 16 KiB PRG bank at $8000, fixed last bank at $C000
//  - 8 x 1 KiB CHR banks
//  - mirroring control
//  - 16-bit down counter IRQ (M2 / CPU cycles)
// EEPROM functionality is not emulated.

class Mapper016 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ; }
    Mapper016(uint8_t prgBanks, uint8_t chrBanks);

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
    uint8_t chrBank[8]{};
    uint8_t mirrorMode = 0xFF;

    // IRQ
    uint16_t irqLatch = 0;
    uint16_t irqCounter = 0;
    bool irqEnabled = false;
    bool irqActive = false;
};
