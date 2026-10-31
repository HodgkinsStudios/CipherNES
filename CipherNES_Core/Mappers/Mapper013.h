#pragma once

#include <cstdint>
#include "Mapper.h"

// Nintendo CPROM (iNES mapper 13): fixed PRG, fixed lower 4 KiB CHR-RAM and
// one of four 4 KiB CHR-RAM banks in the upper pattern table.
class Mapper013 final : public Mapper {
public:
    Mapper013(uint16_t prgBanks, uint16_t chrBanks)
        : Mapper(prgBanks, chrBanks) {
        reset();
    }

    uint32_t capabilities() const override { return MAPPER_CAP_BUS_CONFLICT | MAPPER_CAP_MIRRORING; }
    uint8_t mirroringOverride() const override { return 1; }
    void reset() override { upperChr4 = 0; }
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint8_t upperChr4 = 0;
};
