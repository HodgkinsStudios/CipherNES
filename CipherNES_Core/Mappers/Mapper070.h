#pragma once
#include <cstdint>
#include "Mapper.h"

// iNES mapper 70: Bandai 74161/7432 board, hardwired mirroring from header.
class Mapper070 final : public Mapper {
public:
    Mapper070(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint8_t prgBank = 0;
    uint8_t chrBank = 0;
};
