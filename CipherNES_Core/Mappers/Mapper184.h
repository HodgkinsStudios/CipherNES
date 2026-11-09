#pragma once
#include <cstdint>
#include "Mapper.h"

// iNES mapper 184: Sunsoft-1.
class Mapper184 final : public Mapper {
public:
    Mapper184(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint16_t chrBank[2]{};
};
