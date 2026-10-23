#pragma once

#include "Mapper.h"

// iNES Mapper 33: Taito TC0190FMC (subset of TC0350FMR without IRQ)
class Mapper033 : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    Mapper033(uint8_t prgBanks16k, uint8_t chrBanks8k);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    void reset() override;

private:
    // Mirroring override: 0xFF = none, 0=H,1=V
    uint8_t mirrorMode = 0xFF;
    // PRG is switched in 8 KiB units
    uint32_t prgBanks8k = 0;
    uint8_t prgReg0 = 0; // $8000-$9FFF
    uint8_t prgReg1 = 1; // $A000-$BFFF

    // CHR registers
    uint16_t chr2k0 = 0; // $0000-$07FF (2 KiB units)
    uint16_t chr2k1 = 1; // $0800-$0FFF
    uint8_t chr1k[4] = {0, 1, 2, 3}; // $1000-$1FFF (1 KiB units)
};
