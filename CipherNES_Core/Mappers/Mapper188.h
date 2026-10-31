#pragma once

#include <cstdint>
#include "Mapper.h"

// Bandai Karaoke Studio board (iNES mapper 188).
class Mapper188 final : public Mapper {
public:
    Mapper188(uint16_t prgBanks16k, uint16_t chrBanks8k);
    uint32_t capabilities() const override {
        return MAPPER_CAP_CPU_DIRECT | MAPPER_CAP_MIRRORING | MAPPER_CAP_BUS_CONFLICT;
    }
    void reset() override;
    bool cpuReadDirect(uint16_t addr, uint8_t& data) override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint16_t lowerPrg = 0;
    bool lowerOpenBus = false;
    uint8_t mirrorMode = 1;
};
