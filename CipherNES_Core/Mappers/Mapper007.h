#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 7 (AxROM / AOROM)
//  - 32KB PRG banking at $8000
//  - One-screen mirroring select
class Mapper007 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    Mapper007(uint8_t prgBanks, uint8_t chrBanks);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

private:
    uint8_t prgBank32k = 0;
    uint8_t mirrorMode = 0xFF; // 3=1scA, 4=1scB (Cartridge::Mirror)
};
