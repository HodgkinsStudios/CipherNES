#pragma once

#include <cstdint>
#include "Mapper.h"

// Legacy iNES mapper 130 is a duplicate assignment for the BMC 12-in-1
// board now standardized as NES 2.0 mapper 331.
class Mapper130 final : public Mapper {
public:
    Mapper130(uint16_t prgBanks16k, uint16_t chrBanks8k);

    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint8_t regs[2]{0,0};
    uint8_t mode = 0;
    uint16_t prg16[2]{0,0};
    uint16_t chr4[2]{0,0};
    uint8_t mirrorMode = 1;

    void updateState();
};
