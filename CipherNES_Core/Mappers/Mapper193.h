#pragma once

#include <cstdint>
#include "Mapper.h"

// NTDEC TC-112 (iNES mapper 193).
class Mapper193 final : public Mapper {
public:
    Mapper193(uint16_t prgBanks, uint16_t chrBanks)
        : Mapper(prgBanks, chrBanks) {
        reset();
    }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;

    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint8_t prg = 0;
    uint16_t chr2k[4]{};
};
