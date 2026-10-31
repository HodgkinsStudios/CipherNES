#pragma once
#include <cstdint>
#include "Mapper.h"

// iNES mapper 68: Sunsoft-4, including ROM-backed nametables.
class Mapper068 final : public Mapper {
public:
    Mapper068(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_NAMETABLE; }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    bool usesCpuClock() const override { return true; }
    void cpuClock() override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint16_t chr2k[4]{};
    uint16_t nt1k[2]{};
    uint16_t prgPage = 0;
    uint8_t mirrorMode = 1;
    bool useChrNametables = false;
    bool prgRamEnabled = false;
    bool usingExternalRom = false;
    uint32_t licensingTimer = 0;
    uint8_t ntSelect(uint8_t table) const;
};
