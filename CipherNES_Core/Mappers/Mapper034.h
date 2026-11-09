#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 34 (BNROM/NINA-001 family). This implements the common BNROM
// behavior used by 32KB PRG bank switching test ROMs.
class Mapper034 final : public Mapper {
public:
    Mapper034(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint8_t prgBankSelect = 0;
    uint8_t chrBank0 = 0;
    uint8_t chrBank1 = 1;
};
