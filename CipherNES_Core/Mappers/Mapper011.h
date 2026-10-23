#pragma once

#include <cstdint>
#include "Mapper.h"

// Mapper 11 (Color Dreams)
class Mapper011 final : public Mapper {
public:
    Mapper011(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }

    void reset() override;

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

private:
    uint8_t prgBankSelect = 0; // 32KB units
    uint8_t chrBankSelect = 0; // 8KB units
};
