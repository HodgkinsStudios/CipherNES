#pragma once
#include <cstdint>
#include "Mapper.h"

// iNES mapper 113: HES NTD-8 multicart / NINA-03/06 derivative.
class Mapper113 final : public Mapper {
public:
    Mapper113(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
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
    uint8_t prgBank = 0;
    uint8_t chrBank = 0;
    uint8_t mirrorMode = 0;
};
