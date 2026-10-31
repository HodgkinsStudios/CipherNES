#pragma once

#include "Mapper.h"

// iNES Mapper 99: Vs. System mapper (CNROM-like)
class Mapper099 : public Mapper {
public:
    Mapper099(uint8_t prgBanks16k, uint8_t chrBanks8k);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void reset() override;

private:
    uint8_t bankSel = 0; // bit2 of $4016
    uint32_t prgBanks8k = 0;
};
