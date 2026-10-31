#pragma once

#include <cstdint>
#include "Mapper.h"

class Mapper032 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    Mapper032(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint8_t prg0 = 0;
    uint8_t prg1 = 1;
    uint8_t prgMode = 0;
    uint8_t mirrorMode = 0xFF;
    uint8_t chr[8] = {0,1,2,3,4,5,6,7};
    uint8_t prg8kCount() const { return uint8_t(prgBanks * 2); }
    uint16_t chr1kCount() const { return uint16_t(chrBanks * 8); }
};
