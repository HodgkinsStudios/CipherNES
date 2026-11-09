#pragma once

// Created by Jacob Hodgkins
// Konami VRC2a (iNES mapper 22)

#include <array>
#include <cstdint>
#include "Mapper.h"

class Mapper022 final : public Mapper {
public:
    uint32_t capabilities() const override { return MAPPER_CAP_MIRRORING; }
    Mapper022(uint16_t prgBanks, uint16_t chrBanks) : Mapper(prgBanks, chrBanks) { reset(); }

    void reset() override;
    bool cpuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool cpuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    bool ppuMapRead(uint16_t addr, uint32_t& mappedAddr) override;
    bool ppuMapWrite(uint16_t addr, uint32_t& mappedAddr, uint8_t data) override;
    uint8_t mirroringOverride() const override { return mirrorMode; }
    void saveState(std::vector<uint8_t>& out) const override;
    bool loadState(const uint8_t*& p, const uint8_t* end) override;

private:
    uint16_t prgBankCount8K() const;
    uint16_t chrBankCount1K() const;
    void writeChrNibble(uint8_t logicalRegister, uint8_t data);

    uint8_t prgBank0 = 0;
    uint8_t prgBank1 = 1;
    std::array<uint8_t, 8> chrRegisters{}; // Raw VRC2 values; mapper 22 ignores bit 0.
    uint8_t mirrorMode = 0xFF;
};
