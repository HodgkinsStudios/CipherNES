#pragma once

// Created by Jacob Hodgkins
// Action 53 multicart mapper (iNES mapper 28)

#include <cstdint>
#include "Mapper.h"

class Mapper028 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    Mapper028(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override;
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    void updateMirroringFromBankValue(uint8_t value);
    void calculatePrgBanks(uint16_t& low, uint16_t& high) const;
    uint16_t normalizePrgBank(int bank) const;

    uint8_t selectedRegister = 0;
    uint8_t chrBank = 0;
    uint8_t innerPrgBank = 0x0F;
    uint8_t mode = 0;
    uint8_t outerBank = 0x3F;
};
