#pragma once

#include "Mapper.h"

// iNES Mapper 79: AVE NINA-03 / NINA-06
class Mapper079 : public Mapper {
public:
    Mapper079(uint8_t prgBanks16k, uint8_t chrBanks8k);

    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void reset() override;

private:
    uint8_t prg32kBank = 0;
    uint8_t chr8kBank = 0;
    uint32_t prgBanks32k = 1;
};
