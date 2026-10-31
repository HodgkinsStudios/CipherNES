#pragma once

#include <cstdint>
#include "Mapper.h"

// Legacy iNES mapper 160 assignment used by FCEUX: Sachen SA-009.
// Fixed 32 KiB PRG and one-bit 8 KiB CHR banking. The register is decoded
// in $4100-$5FFF only when CPU A8 is high.
class Mapper160 final : public Mapper {
public:
    Mapper160(uint16_t prgBanks16k, uint16_t chrBanks8k);
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint8_t chrBank = 0;
};
