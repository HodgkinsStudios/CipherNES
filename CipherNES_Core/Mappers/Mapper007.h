#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 7 (AxROM / AOROM)
//  - 32KB PRG banking at $8000
//  - One-screen mirroring select
class Mapper007 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | ((submapper == 2) ? MAPPER_CAP_BUS_CONFLICT : 0u); }
    Mapper007(uint8_t prgBanks, uint8_t chrBanks, uint8_t submapper = 0);

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    uint8_t mirroringOverride() const override { return mirrorMode; }

    uint32_t saveStateVersion() const override { return 2u; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    const uint8_t submapper;
    uint8_t prgBank32k = 0;
    uint8_t mirrorMode = 0xFF; // 3=1scA, 4=1scB (Cartridge::Mirror)
};
