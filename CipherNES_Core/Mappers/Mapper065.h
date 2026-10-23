#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 65 (Irem H3001)
//  - PRG: 8KB banks, fixed last bank at $E000, fixed second-last at either $8000 or $C000
//  - CHR: 1KB banks via $B000-$B007
//  - Mirroring via $9001
//  - Cycle IRQ via a 16-bit downcounter
class Mapper065 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_IRQ; }
    Mapper065(uint8_t prgBanks, uint8_t chrBanks);

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
    // PRG banking (8KB units)
    uint8_t prgReg0 = 0; // $8000
    uint8_t prgReg1 = 1; // $A000
    bool prgSwap = false; // $9000 bit7? actually bit? documented as X... .... so bit7

    // CHR banking (1KB)
    uint8_t chrReg[8]{};

    // Mirroring override
    uint8_t mirrorMode = 0xFF;

    // IRQ
    bool irqEnabled = false;
    bool irqActive = false;
    uint16_t irqReload = 0;
    uint16_t irqCounter = 0;

    uint8_t prgBankCount8k() const;
    uint16_t chrBankCount1k() const;
};
