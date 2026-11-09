#pragma once

#include <cstdint>
#include "Mapper.h"

class Mapper010 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING | MAPPER_CAP_PPU_ADDR_LATCH; }
    Mapper010(uint8_t prgBanks, uint8_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }
    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    void ppuAddressLatch(uint16_t addr) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;
private:
    uint8_t prgBank16k = 0;
    uint8_t chrFD_0000 = 0;
    uint8_t chrFE_0000 = 0;
    uint8_t chrFD_1000 = 0;
    uint8_t chrFE_1000 = 0;
    uint8_t latch0 = 0xFE;
    uint8_t latch1 = 0xFE;
    uint8_t mirrorMode = 1;

    uint8_t chr4kBankCount() const { return uint8_t(chrBanks * 2); }
    uint32_t mapChr4k(uint8_t bank4k, uint16_t addr) const;
};
